#include "main.h"

using namespace std;

static Connect* list_start = NULL, * list_end = NULL;

static int max_resp = 0;

fd_set wrfds;

mutex mtx_send;
condition_variable cond_add;
condition_variable cond_minus;

int count_resp = 0;
int close_thr = 0;
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

    ret = send_file_2(req->clientSocket, req->resp.fd, rd_buf, len, req->resp.offset);
    if (ret <= 0)
    {
        if (ret == -1)
            print_err("%d<%s:%d> Error: Sent %lld bytes\n", req->numChld, __func__, __LINE__, req->resp.send_bytes);
        return ret;
    }

    req->resp.send_bytes += ret;
    req->resp.offset += ret;
    req->resp.respContentLength -= ret;
    if (req->resp.respContentLength == 0)
        ret = 0;

    return ret;
}
//======================================================================
void del_from_list(Connect* r, RequestManager* ReqMan)
{
mtx_send.lock();

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
mtx_send.unlock();
    _close(r->resp.fd);
    ReqMan->close_response(r);
}
//======================================================================
int set_list(RequestManager* ReqMan)
{
    int i = 0;
    time_t t = time(NULL);
    Connect* tmp = list_start, *next;

    for (; tmp; tmp = next)
    {
        next = tmp->next;

        if (((t - tmp->time_write) > conf->TimeOut) && (tmp->time_write != 0))
        {
            tmp->err = -1;
            print_err("%d<%s:%d> Timeout = %ld\n", tmp->numChld, __func__, __LINE__, t - tmp->time_write);
            tmp->req_hdrs.iReferer = NUM_HEADERS - 1;
            tmp->req_hdrs.Value[tmp->req_hdrs.iReferer] = (char*)"Timeout";
            del_from_list(tmp, ReqMan);
        }
        else
        {
            if (tmp->time_write == 0)
                tmp->time_write = t;

            FD_SET(tmp->clientSocket, &wrfds);
            ++i;
        }

        if (i >= max_resp)
            break;
    }

    return i;
}
//======================================================================
void delete_timeout_requests(int n, RequestManager* ReqMan)
{
    time_t t = time(NULL);
    Connect* tmp = list_start, *next;

    for (; tmp && (n > 0); tmp = next)
    {
        next = tmp->next;
        if ((t - tmp->time_write) > conf->TimeOut)
        {
            tmp->err = -1;
            print_err("%d<%s:%d> Timeout = %ld\n", tmp->numChld, __func__, __LINE__, t - tmp->time_write);
            tmp->req_hdrs.iReferer = NUM_HEADERS - 1;
            tmp->req_hdrs.Value[tmp->req_hdrs.iReferer] = (char*)"Timeout";
            del_from_list(tmp, ReqMan);
        }
        
        --n;
    }
}
//======================================================================
void send_files(RequestManager * ReqMan)
{
    int i, ret = 0;
    int size_buf = conf->SOCK_BUFSIZE;
    struct timeval tv;
    int num_chld = ReqMan->get_num_chld();
    char* rd_buf;

    max_resp = FD_SETSIZE;

    rd_buf = new(nothrow) char [size_buf];
    if (!rd_buf)
    {
        print_err("%d<%s:%d> Error malloc()\n", num_chld, __func__, __LINE__);
        exit(1);
    }

    FD_ZERO(&wrfds);

    i = 0;
    while (1)
    {
        {
            unique_lock<mutex> lk(mtx_send);
            while ((list_start == NULL) && (!close_thr))
            {
                count_resp = 0;
                cond_add.wait(lk);
            }

            if (close_thr)
                break;
            count_resp = set_list(ReqMan);
            if (count_resp == 0)
                continue;
        }

        tv.tv_sec = 1;
        tv.tv_usec = 0;
        ret = select(0, NULL, &wrfds, NULL, &tv);
        if (ret == SOCKET_ERROR)
        {
            DWORD err = WSAGetLastError();
            print_err("%d<%s:%d> Error select(): %d, num_select=%d\n", num_chld, __func__, __LINE__, err, count_resp);
            exit(1); // ?
        }
        else if (ret == 0)
        {
            delete_timeout_requests(count_resp, ReqMan);
            continue;
        }
        
        i = 0;
        Connect *tmp = list_start, *next;
        while ((i < count_resp) && (ret > 0) && tmp)
        {
            next = tmp->next;
            if (FD_ISSET(tmp->clientSocket, &wrfds))
            {
                --ret;
                FD_CLR(tmp->clientSocket, &wrfds);
                int wr = send_entity(tmp, rd_buf, size_buf);
                if (wr == 0)
                {
                    tmp->err = wr;
                    del_from_list(tmp, ReqMan);
                }
                else if (wr == -1)
                {
                    tmp->err = wr;
                    tmp->req_hdrs.iReferer = NUM_HEADERS - 1;
                    tmp->req_hdrs.Value[tmp->req_hdrs.iReferer] = (char*)"Connection reset by peer";
                    del_from_list(tmp, ReqMan);
                }
                else if (wr > 0)
                {
                    tmp->time_write = 0;
                }
          //      else; // (wr < -1)
            }
            tmp = next;

            ++i;
        }
    }

    delete [] rd_buf;
}
//======================================================================
void push_resp_queue(Connect* req)
{
    req->free_resp_headers();
    req->free_range();
mtx_send.lock();
    req->time_write = 0;
    req->next = NULL;
    req->prev = list_end;
    if (list_start)
    {
        list_end->next = req;
        list_end = req;
    }
    else
        list_start = list_end = req;
mtx_send.unlock();
    cond_add.notify_one();
}
//======================================================================
void close_queue(void)
{
    close_thr = 1;
    cond_add.notify_one();
}
