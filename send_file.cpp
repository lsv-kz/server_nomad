#include "main.h"

using namespace std;

request* list_start = NULL;
request* list_end = NULL;

fd_set wrfds;

mutex mtx_send;
condition_variable cond_add;
condition_variable cond_minus;
int count_resp = 0;
int close_thr = 0;
int num_select = 0;
/*====================================================================*/
int send_entity(request* req, char* rd_buf, int size_buf)
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
request* del_from_list(request* r, RequestManager* ReqMan)
{
    request* tmp, * prev = r->prev;
    mtx_send.lock();
    if ((r->prev) && (r->next))
    {
        tmp = r->prev;
        tmp->next = r->next;

        tmp = r->next;
        tmp->prev = r->prev;
    }
    else if ((!r->prev) && (r->next))
    {
        list_start = r->next;
        list_start->prev = NULL;
    }
    else if ((r->prev) && (!r->next))
    {
        list_end = r->prev;
        list_end->next = NULL;
    }
    else // (!r->prev) && (!r->next)
    {
        list_start = list_end = NULL;
        if (count_resp != 1)
            print_err("%d<%s:%d> Error: count_resp != 1\n", r->numChld, __func__, __LINE__);
    }

    --count_resp;
    mtx_send.unlock();
    _close(r->resp.fd);
    ReqMan->close_response(r);
    cond_minus.notify_one();

    return prev;
}
//======================================================================
int set_list()
{
    int i = 0;
    time_t t = time(NULL);
    request* tmp = list_start;

    for (; tmp; tmp = tmp->next)
    {
        if (tmp->time_write == 0)
            tmp->time_write = t;
        FD_SET(tmp->clientSocket, &wrfds);
        ++i;
    }
    num_select = i;
    return 0;
}
//======================================================================
void delete_timeout_requests(RequestManager* ReqMan)
{
    time_t t = time(NULL);
    request* tmp = list_start, * r;

    for (; tmp; tmp = tmp->next)
    {
        if ((t - tmp->time_write) > conf->TimeOut)
        {
            r = tmp;
            tmp = tmp->prev;
            print_err("%d<%s:%d> Timeout = %ld\n", r->numChld, __func__, __LINE__, t - r->time_write);
            r->req_hdrs.iReferer = NUM_HEADERS - 1;
            r->req_hdrs.Value[r->req_hdrs.iReferer] = (char*)"Timeout";
            del_from_list(r, ReqMan);
        }
    }
}
//======================================================================
void send_files(RequestManager * ReqMan)
{
    int i, ret = 0;
    int timeout = 1;
    int size_buf = conf->SOCK_BUFSIZE;
    time_t time_write;
    struct timeval tv;
    request* tmp;
    char* rd_buf;

    rd_buf = new(nothrow) char [size_buf];
    if (!rd_buf)
    {
        print_err("%d<%s:%d> Error malloc(): %d\n", ReqMan->get_num_chld(), __func__, __LINE__, errno);
        exit(1);
    }

    FD_ZERO(&wrfds);

    i = 0;
    while (1)
    {
        {
            unique_lock<mutex> lk(mtx_send);
            while ((count_resp == 0) && (!close_thr))
                cond_add.wait(lk);

            if (close_thr)
                break;
            set_list();
        }

        tv.tv_sec = timeout;
        tv.tv_usec = 0;
        ret = select(0, NULL, &wrfds, NULL, &tv);
        if (ret == -1)
        {
            print_err("%d<%s:%d> Error select(): %d\n", ReqMan->get_num_chld(), __func__, __LINE__, WSAGetLastError());
            exit(1);
        }
        else if (ret == 0)
        {
            delete_timeout_requests(ReqMan);
            continue;
        }

        time_write = time(NULL);
        
        i = 0;
        tmp = list_start;
        while ((i < num_select) && (ret > 0) && tmp)
        {
            if (FD_ISSET(tmp->clientSocket, &wrfds))
            {
                FD_CLR(tmp->clientSocket, &wrfds);
                int wr = send_entity(tmp, rd_buf, size_buf);
                if (wr == 0)
                {
                    tmp->err = wr;
                    tmp = del_from_list(tmp, ReqMan);
                }
                else if (wr == -1)
                {
                    tmp->err = wr;
                    tmp->req_hdrs.iReferer = NUM_HEADERS - 1;
                    tmp->req_hdrs.Value[tmp->req_hdrs.iReferer] = (char*)"Connection reset by peer";
                    tmp = del_from_list(tmp, ReqMan);
                }
                else // (wr < -1) || (wr > 0)
                {
                    tmp->time_write = 0;
                }
                --ret;
            }
            else
            {
                time_t t = time_write - tmp->time_write;
                if (t > conf->TimeOut)
                {
                    print_err("%d<%s:%d> Timeout = %ld\n", ReqMan->get_num_chld(), __func__, __LINE__, t);
                    tmp->req_hdrs.iReferer = NUM_HEADERS - 1;
                    tmp->req_hdrs.Value[tmp->req_hdrs.iReferer] = (char*)"Timeout";
                    tmp->err = -1;
                    tmp = del_from_list(tmp, ReqMan);
                }
            }

            if (!tmp)
                break;
            else
                tmp = tmp->next;

            ++i;
        }
    }

    delete [] rd_buf;
}
//======================================================================
void push_resp_queue(request * req)
{
    req->free_resp_headers();
    req->free_range();
    unique_lock<mutex> lk(mtx_send);
    while (count_resp >= conf->SizeQueue)
    {
        print_err("%d<%s:%d>  wait(); size_conv=%d\n", req->numChld, __func__, __LINE__, count_resp);
        cond_minus.wait(lk);
    }

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
    ++count_resp;

    cond_add.notify_one();
}
//======================================================================
void close_conv(void)
{
    close_thr = 1;
    cond_add.notify_one();
}
