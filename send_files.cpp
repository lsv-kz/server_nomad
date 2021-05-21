#include "main.h"

// POLLERR=0x1, POLLHUP=0x2, POLLNVAL=0x4, POLLPRI=0x400, POLLRDBAND=0x200
// POLLRDNORM=0x100, POLLWRNORM=0x10, POLLIN=0x300, POLLOUT=0x10
// 0x13, 0x2, 0x12
using namespace std;

static Connect* list_start = NULL;
static Connect* list_end = NULL;

static Connect* list_new_start = NULL;
static Connect* list_new_end = NULL;

static PWSAPOLLFD fdwr;

static mutex mtx_send;
static condition_variable cond_add;
static condition_variable cond_minus;

static int count_resp = 0;
static int close_thr = 0;
/*====================================================================*/
int send_entity(Connect* req, char* rd_buf, int size_buf)
{
    int ret;
    int len;

    if (req->resp.respContentLength >= (long long)size_buf)
        len = size_buf;
    else
    {
        len = (int)req->resp.respContentLength;
        if (len == 0)
            return 0;
    }

    ret = send_file_2(req->clientSocket, req->resp.fd, rd_buf, len);
    if (ret <= 0)
    {
        if (ret == -1)
            print_err(req, "<%s:%d> Error: Sent %lld bytes\n", __func__, __LINE__, req->resp.send_bytes);
        return ret;
    }

    req->resp.send_bytes += ret;
    req->resp.respContentLength -= ret;
    if (req->resp.respContentLength == 0)
        ret = 0;

    return ret;
}
//======================================================================
void del_from_list(Connect* r, RequestManager * ReqMan)
{
    if (r->prev && r->next)
    {
        r->prev->next = r->next;
        r->next->prev = r->prev;
    }
    else if (r->prev && !r->next)
    {
        r->prev->next = r->next;
        list_end = r->prev;
    }
    else if (!r->prev && r->next)
    {
        r->next->prev = r->prev;
        list_start = r->next;
    }
    else if (!r->prev && !r->next)
    {
        list_start = list_end = NULL;
    }

    _close(r->resp.fd);
    ReqMan->end_response(r);
}
//======================================================================
int set_list(RequestManager * ReqMan)
{
    mtx_send.lock();
    if (list_new_start)
    {
        if (list_end)
            list_end->next = list_new_start;
        else
            list_start = list_new_start;

        list_new_start->prev = list_end;
        list_end = list_new_end;
        list_new_start = list_new_end = NULL;
    }
    mtx_send.unlock();

    int i = 0;
    __time64_t t = _time64(NULL);
    Connect* tmp = list_start, * next;

    for (; tmp; tmp = next)
    {
        next = tmp->next;

        if (((t - tmp->time_write) > conf->TimeOut) && (tmp->time_write != 0))
        {
            tmp->err = -1;
            print_err("%d<%s:%d> Timeout = %ld\n", tmp->numChld, __func__, __LINE__, t - tmp->time_write);
            tmp->req_hdrs.iReferer = NUM_HEADERS - 1;
            tmp->req_hdrs.Value[tmp->req_hdrs.iReferer] = "Timeout";
            del_from_list(tmp, ReqMan);
        }
        else
        {
            if (tmp->time_write == 0)
                tmp->time_write = t;

            tmp->index_fdwr = i;
            fdwr[i].fd = tmp->clientSocket;
            fdwr[i].events = POLLWRNORM;
            ++i;
        }

        if (i >= conf->MaxRequests)
            break;
    }

    return i;
}
//======================================================================
void send_files(RequestManager * ReqMan)
{
    int i, ret = 0;
    int timeout = 100;
    int size_buf = conf->SOCK_BUFSIZE;
    char* rd_buf;
    int num_chld = ReqMan->get_num_chld();

    fdwr = new(nothrow) WSAPOLLFD [conf->MaxRequests];
    rd_buf = new(nothrow) char[size_buf];
    if (!rd_buf || !fdwr)
    {
        print_err("[%d]<%s:%d> Error malloc()\n", num_chld, __func__, __LINE__);
        exit(1);
    }

    memset(fdwr, 0, sizeof(WSAPOLLFD) * conf->MaxRequests);

    i = 0;
    while (1)
    {
        {
            unique_lock<mutex> lk(mtx_send);
            while ((list_start == NULL) && (list_new_start == NULL) && (!close_thr))
            {
                count_resp = 0;
                cond_add.wait(lk);
            }

            if (close_thr)
                break;
        }

        count_resp = set_list(ReqMan);
        if (count_resp == 0)
            continue;

        ret = WSAPoll(fdwr, count_resp, timeout);
        if (ret == SOCKET_ERROR)
        {
            print_err("[%d]<%s:%d> Error WSAPoll(): %d\n", num_chld, __func__, __LINE__, WSAGetLastError());
            exit(1);
        }
        else if (ret == 0)
        {
            continue;
        }

        i = 0;
        Connect* req = list_start, * next;
        for (; (i < count_resp) && (ret > 0) && req; req = next)
        {
            next = req->next;
            if (fdwr[req->index_fdwr].revents == POLLWRNORM)
            {
                --ret;
                int wr = send_entity(req, rd_buf, size_buf);
                if (wr == 0)
                {
                    req->err = wr;
                    del_from_list(req, ReqMan);
                }
                else if (wr == -1)
                {
                    req->err = wr;
                    req->req_hdrs.iReferer = NUM_HEADERS - 1;
                    req->req_hdrs.Value[req->req_hdrs.iReferer] = "Connection reset by peer";
                    del_from_list(req, ReqMan);
                }
                else if (wr > 0)
                {
                    req->time_write = 0;
                }
            }
            else if (fdwr[req->index_fdwr].revents != 0)
            {
                --ret;
                print_err(req, "<%s:%d> revents=0x%x\n", __func__, __LINE__, fdwr[req->index_fdwr].revents);
                req->err = -1;
                req->req_hdrs.iReferer = NUM_HEADERS - 1;
                req->req_hdrs.Value[req->req_hdrs.iReferer] = "Connection reset by peer";
                del_from_list(req, ReqMan);
            }

            ++i;
        }
    }
    print_err("%d<%s:%d> ***** \n", num_chld, __func__, __LINE__);

    delete[] rd_buf;
    delete[] fdwr;
}
//======================================================================
void push_send_list(Connect* req)
{
    _lseeki64(req->resp.fd, req->resp.offset, SEEK_SET);
    req->time_write = 0;
    req->next = NULL;
 mtx_send.lock();
    req->prev = list_new_end;
    if (list_new_start)
    {
        list_new_end->next = req;
        list_new_end = req;
    }
    else
        list_new_start = list_new_end = req;

 mtx_send.unlock();
    cond_add.notify_one();
}
//======================================================================
void close_send_list(void)
{
    close_thr = 1;
    cond_add.notify_one();
}
