#include "main.h"

using namespace std;

static request** Qu;
static int size_qu = 0;

fd_set wrfds;

int index_new_resp = 0;

mutex mtx_send;
condition_variable cond_add;
condition_variable cond_shift;
int count_resp = 0;
int close_thr = 0;
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
int shift_queue()
{
    int i = 0, i_empty = 0, num_select = 0;
    time_t t = time(NULL);

    while (i < index_new_resp)
    {
        if (Qu[i] != NULL)
        {
            if (Qu[i]->time_write == 0)
                Qu[i]->time_write = t;
            if (num_select < FD_SETSIZE)
            {
                FD_SET(Qu[i]->clientSocket, &wrfds);
                ++num_select;
            }
            else
            {
                print_err("<%s:%d> num_select= %d, count_resp=%d\n", __func__, __LINE__, num_select, count_resp);
                return num_select;
            }

            if (i > i_empty)
            {
                Qu[i_empty] = Qu[i];
                Qu[i] = NULL;
            }

            ++i_empty;
        }

        ++i;
    }

    index_new_resp = i_empty;
 
    if (num_select != i_empty)
    {
        print_err("<%s:%d> %d != %d\n", __func__, __LINE__, num_select, i_empty);
        
    }
    if (num_select == 0)
            print_err("<%s:%d> num_select= %d, count_resp=%d\n", __func__, __LINE__, num_select, count_resp);
    
    return i_empty;
}
//======================================================================
void delete_request(int i, RequestManager* ReqMan)
{
    _close(Qu[i]->resp.fd);
    ReqMan->close_response(Qu[i]);
    mtx_send.lock();
    Qu[i] = NULL;
    --count_resp;
    mtx_send.unlock();
}
//======================================================================
void delete_timeout_requests(int n, RequestManager* ReqMan)
{
    int i = 0;
    time_t t = time(NULL);

    while (i < n)
    {
        if (Qu[i] != NULL)
        {
            if ((t - Qu[i]->time_write) > conf->TimeOut)
            {
                print_err("%d<%s:%d> Timeout = %ld\n", Qu[i]->numChld, __func__, __LINE__, t - Qu[i]->time_write);
                _close(Qu[i]->resp.fd);
                Qu[i]->req_hdrs.iReferer = NUM_HEADERS - 1;
                Qu[i]->req_hdrs.Value[Qu[i]->req_hdrs.iReferer] = (char*)"Timeout";
                ReqMan->close_response(Qu[i]);
                mtx_send.lock();
                Qu[i] = NULL;
                --count_resp;
                mtx_send.unlock();
            }
        }
        ++i;
    }
}
//======================================================================
void delete_requests(int n, RequestManager* ReqMan)
{
    int i = 0;

    while (i < n)
    {
        if (Qu[i] != NULL)
        {
            _close(Qu[i]->resp.fd);
            ReqMan->close_response(Qu[i]);
        mtx_send.lock();
            Qu[i] = NULL;
            --count_resp;
        mtx_send.unlock();
        }
        ++i;
    }
}
//======================================================================
void send_files(RequestManager * ReqMan)
{
    int i, ret = 0;
    int num_select, timeout = 1;
    int size_buf = conf->SOCK_BUFSIZE;
    time_t time_write;
    struct timeval tv;
    char* rd_buf;
    int num_chld = ReqMan->get_num_chld();
    size_qu = FD_SETSIZE; //  conf->SizeQueue 
    print_err("%d<%s:%d> fd_set: FD_SETSIZE=%d; size_qu=%d\n", num_chld, __func__, __LINE__, FD_SETSIZE, conf->SizeQueue);
    
    Qu = new(nothrow) request * [size_qu];
    rd_buf = new(nothrow) char[size_buf];
    if (!Qu || !rd_buf)
    {
        print_err("%d<%s:%d> Error malloc(): %d\n", num_chld, __func__, __LINE__, errno);
        exit(1);
    }

    FD_ZERO(&wrfds);

    i = 0;
    while (i < size_qu)
    {
        Qu[i] = NULL;
        ++i;
    }

    i = 0;
    while (1)
    {
        {
            unique_lock<mutex> lk(mtx_send);
            while ((count_resp == 0) && (!close_thr))
                cond_add.wait(lk);

            if (close_thr)
                break;
            num_select = shift_queue();
        }
        cond_shift.notify_one();
        if (num_select == 0)
        {
            print_err("<%s:%d> num_select=%d\n", __func__, __LINE__, num_select);
      //      print_err("<%s:%d> num_select=%d, count_resp=%d\n", __func__, __LINE__, num_select, count_resp);
        }

        tv.tv_sec = 1;
        tv.tv_usec = 0;
        ret = select(0, NULL, &wrfds, NULL, &tv);
        if (ret == SOCKET_ERROR)
        {
            DWORD err = WSAGetLastError();
            print_err("%d<%s:%d> Error select(): %d, num_select=%d\n", num_chld, __func__, __LINE__, err, num_select);
            if (err == WSAEMFILE)
            {
                
                delete_requests(num_select, ReqMan);
                continue;
            }
            
            exit(1);
        }
        else if (ret == 0)
        {
            delete_timeout_requests(num_select, ReqMan);
            continue;
        }

        time_write = time(NULL);
        
        i = 0;
        while ((i < num_select) && (ret > 0))
        {
            if (*(Qu+i))
            {
                if (FD_ISSET(Qu[i]->clientSocket, &wrfds))
                {
                    FD_CLR(Qu[i]->clientSocket, &wrfds);
                    int wr = send_entity(Qu[i], rd_buf, size_buf);
                    if (wr == 0)
                    {
                        Qu[i]->err = wr;
                        delete_request(i, ReqMan);
                    }
                    else if (wr == -1)
                    {
                        Qu[i]->err = wr;
                        Qu[i]->req_hdrs.iReferer = NUM_HEADERS - 1;
                        Qu[i]->req_hdrs.Value[Qu[i]->req_hdrs.iReferer] = (char*)"Connection reset by peer";
                        delete_request(i, ReqMan);
                    }
                    else // (wr < -1) || (wr > 0)
                    {
                        Qu[i]->time_write = 0;
                    }
                    --ret;
                }
                else
                {
                    time_t t = time_write - Qu[i]->time_write;
                    if (t > conf->TimeOut)
                    {
                        print_err("%d<%s:%d> Timeout = %ld\n", num_chld, __func__, __LINE__, t);
                        Qu[i]->req_hdrs.iReferer = NUM_HEADERS - 1;
                        Qu[i]->req_hdrs.Value[Qu[i]->req_hdrs.iReferer] = (char*)"Timeout";
                        Qu[i]->err = -1;
                        delete_request(i, ReqMan);
                    }
                }
            }
            ++i;
        }
    }

    delete [] rd_buf;
    delete[] Qu;
}
//======================================================================
void push_resp_queue(request * req)
{
    req->free_range();
    req->free_resp_headers();
    {
        unique_lock<mutex> lk(mtx_send);
        while (index_new_resp >= size_qu)
        {
            cond_shift.wait(lk);
        }

        req->time_write = 0;
        Qu[index_new_resp] = req;
        ++count_resp;
        ++index_new_resp;
    }
    cond_add.notify_one();
}
//======================================================================
void close_queue(void)
{
    close_thr = 1;
    cond_add.notify_one();
}
