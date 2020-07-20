#include "main.h"

using namespace std;

request* create_req(RequestManager* ReqMan);
//======================================================================
RequestManager::RequestManager(int n, HANDLE pipe_out)
{
    list_begin = list_end = NULL;
    len_qu = stop_manager = all_thr = 0;
    count_thr = count_req = num_wait_thr = 0;
    numChld = n;
    hClose_out = pipe_out;
}
//----------------------------------------------------------------------
RequestManager::~RequestManager()
{
    ;
}
//----------------------------------------------------------------------
int RequestManager::get_num_chld(void)
{
    return numChld;
}
//----------------------------------------------------------------------
int RequestManager::get_num_thr(void)
{
  mtx_thr.lock();
    int ret = count_thr;
  mtx_thr.unlock();
    return ret;
}
//----------------------------------------------------------------------
int RequestManager::get_all_thr(void)
{
    return all_thr;
}
//----------------------------------------------------------------------
int RequestManager::get_num_req(void)
{
    mtx_thr.lock();
    int ret = count_req;
    mtx_thr.unlock();
    return ret;
}
//----------------------------------------------------------------------
int RequestManager::start_thr(void)
{
  mtx_thr.lock();
    int ret = ++count_thr;
    ++all_thr;
  mtx_thr.unlock();
    return ret;
}
//----------------------------------------------------------------------
int RequestManager::exit_thr()
{
  mtx_thr.lock();
    int ret = --count_thr;
  mtx_thr.unlock();
    cond_exit_thr.notify_one();
    return ret;
}
//----------------------------------------------------------------------
void RequestManager::wait_exit_thr(int n)
{
    unique_lock<mutex> lk(mtx_thr);
    while (n == count_thr)
    {
        cond_exit_thr.wait(lk);
    }
}
//----------------------------------------------------------------------
void RequestManager::timedwait_close_req(void)
{
    unique_lock<mutex> lk(mtx_close_req);
    cond_close_req.wait_for(lk, chrono::milliseconds(1000 * conf->TimeoutThreadCond));
}
//----------------------------------------------------------------------
int RequestManager::push_req(request* req, int inc)
{
    int ret;
    {
        unique_lock<mutex> lk(mtx_thr);
        while (count_req >= conf->MaxRequests) // =====================================
        {
            cond_close_req.wait(lk);
        }

        req->next = NULL;
        req->prev = list_end;
        if (list_begin)
        {
            list_end->next = req;
            list_end = req;
        }
        else
            list_begin = list_end = req;

        if (inc)
            ++count_req;
        ret = ++len_qu;
    }

    cond_push.notify_one();

    return ret;
}
//----------------------------------------------------------------------
request* RequestManager::pop_req()
{
    request* req;
    unique_lock<mutex> lk(mtx_thr);
    ++num_wait_thr;
    while (list_begin == NULL)
    {
        cond_push.wait(lk);
    }
    --num_wait_thr;
    req = list_begin;
    if (list_begin->next)
    {
        list_begin->next->prev = NULL;
        list_begin = list_begin->next;
    }
    else
        list_begin = list_end = NULL;

    --len_qu;
    cond_create_thr.notify_one();
    return req;
}
//----------------------------------------------------------------------
int RequestManager::end_req(int* nthr, int* nreq)
{
    int ret = 0;
    mtx_thr.lock();

    if ((num_wait_thr > len_qu) && (count_thr > conf->MinThreads))
    {
        --count_thr;
        ret = EXIT_THR;
    }

    *nthr = count_thr;
    *nreq = count_req;
    mtx_thr.unlock();
    if (ret)
        cond_exit_thr.notify_all();
    return ret;
}
//----------------------------------------------------------------------
int RequestManager::get_len_qu(void)
{
  mtx_thr.lock();
    int ret = len_qu;
  mtx_thr.unlock();
    return ret;
}
//--------------------------------------------
void RequestManager::close_manager()
{
    stop_manager = 1;
    cond_create_thr.notify_one();
}
//----------------------------------------------------------------------
void RequestManager::close_response(request * req)
{
    if (req->connKeepAlive == 0) // || req->err < 0)
    {
        //   if (req->err != -1)
        print_log(req);
        //----------------- close connect ------------------------------
        shutdown(req->clientSocket, SD_BOTH);
        closesocket(req->clientSocket);
        delete req;
      mtx_thr.lock();
        --count_req;
      mtx_thr.unlock();
        cond_close_req.notify_all();
        unsigned char ch = (unsigned char)numChld;
        DWORD rd;
        bool res = WriteFile(hClose_out, &ch, 1, &rd, NULL);
        if (!res)
        {
            PrintError(__func__, __LINE__, "Error WriteFile()");
            exit(1);
        }
    }
    else
    {
        print_log(req);
        ++req->numReq;
        push_req(req, 0);
    }
}
//----------------------------------------------------------------------
int RequestManager::wait_create_thr(int* n)
{
    {
        unique_lock<mutex> lk(mtx_thr);
        while (1)
        {    // ?
            while ((num_wait_thr >= len_qu) || ((count_req - len_qu) >= conf->SizeQueue))
                //   while ((num_wait_thr >= len_qu))// || (((count_req - len_qu) * 2 + len_qu) >= 1000))
            {
                if (stop_manager)
                    break;
                cond_create_thr.wait(lk);
            }

            while ((count_thr >= conf->MaxThreads) && !stop_manager)
            {
                cond_exit_thr.wait(lk);
            }

            if ((num_wait_thr < len_qu) && ((count_req - len_qu) < conf->SizeQueue))
                //   if ((num_wait_thr < len_qu))// && (((count_req - len_qu) * 2 + len_qu) < 1000))
                break;
        }

        *n = count_thr;
    }

    return stop_manager;
}
//======================================================================
void thread_req_manager(int numProc, RequestManager * ReqMan)
{
    int num_thr;
    thread thr;

    while (1)
    {
        if (ReqMan->wait_create_thr(&num_thr))
            break;

        try
        {
            thr = thread(get_request, ReqMan);
        }
        catch (...)
        {
            print_err("%d<%s:%d> Error create thread: num_thr=%d, errno=%d\n", numProc, __func__, __LINE__, num_thr);
            ReqMan->wait_exit_thr(num_thr);
            continue;
        }

        thr.detach();

        ReqMan->start_thr();
    }
    print_err("%d<%s:%d> Exit thread_req_manager()\n", numProc, __func__, __LINE__);
}
//======================================================================
SOCKET request::serverSocket;
//======================================================================
void child_proc(SOCKET sockServer, int numChld, HANDLE hParent, HANDLE hClose_out)
{
    int n;
    int allNumThr = 0;
    unsigned long allConn = 0;
    RequestManager* ReqMan;

    WSADATA WsaDat;
    int err = WSAStartup(MAKEWORD(2, 2), &WsaDat);
    if (err != 0)
    {
        print_err("<%s:%d> WSAStartup failed with error: %d\n", __func__, __LINE__, err);
        exit(1);
    }
    //------------------------------------------------------------------
    if (_wchdir(conf->wRootDir.c_str()))
    {
        print_err("%d<%s:%d> Error root_dir: %d\n", numChld, __func__, __LINE__, errno);
        exit(1);
    }

    request::serverSocket = sockServer;
    //------------------------------------------------------------------
    ReqMan = new(nothrow) RequestManager(numChld, hClose_out);
    if (!ReqMan)
    {
        print_err("<%s:%d> *********** Exit child %d ***********\n", __func__, __LINE__, numChld);
        exit(1);
    }
    //------------------------------------------------------------------
    thread SendFile;
    try
    {
        SendFile = thread(send_files, ReqMan);
    }
    catch (...)
    {
        print_err("%d<%s:%d> Error create thread(send_file_)\n", numChld, __func__, __LINE__);
        exit(1);
    }
    //------------------------------------------------------------------
    n = 0;
    while (n < conf->MinThreads)
    {
        thread thr;
        try
        {
            thr = thread(get_request, ReqMan);
        }
        catch (...)
        {
            print_err("%d<%s:%d> Error create thread\n", numChld, __func__, __LINE__);
            exit(1);
        }
        ++allNumThr;
        ReqMan->start_thr();
        thr.detach();
        ++n;
    }

    //------------------------------------------------------------------
    thread thrReqMan;
    try
    {
        thrReqMan = thread(thread_req_manager, numChld, ReqMan);
    }
    catch (...)
    {
        print_err("<%s:%d> Error create thread %d\n", __func__, __LINE__, allNumThr);
        exit(1);
    }

    while (1)
    {
        unsigned char ch;
        socklen_t addrSize;
        struct sockaddr_storage clientAddr;
       
        DWORD rd;
        bool res;
        res = ReadFile(hParent, &ch, 1, &rd, NULL);
        if (!res)
        {
            PrintError(__func__, __LINE__, "Error ReadFile()");
            break;
        }

        if (ch != (unsigned char)numChld)
        {
            WriteFile(hParent, &ch, 1, &rd, NULL);
            print_err("%d<%s:%d> [ch != numChld] ch=%d\n", numChld, __func__, __LINE__, (int)ch);
            break;
        }

        addrSize = sizeof(struct sockaddr_storage);
        SOCKET clientSocket = accept(sockServer, (struct sockaddr*) & clientAddr, &addrSize);
        if (clientSocket == INVALID_SOCKET)
        {
            int err = ErrorStrSock(__func__, __LINE__, "Error accept()");
            ch = 0x80;
            res = WriteFile(hParent, &ch, 1, &rd, NULL);
            if (!res)
            {
                PrintError(__func__, __LINE__, "Error WriteFile()");
                break;
            }

            print_err("%d<%s:%d> Error accept(): %d\n", numChld, __func__, __LINE__, err);
            if (err == WSAEMFILE)
            {
                ReqMan->timedwait_close_req();
                continue;
            }
            else
            {
                break;
            }
        }

        res = WriteFile(hParent, &ch, 1, &rd, NULL);
        if (!res)
        {
            PrintError(__func__, __LINE__, "Error WriteFile()");
            break;
        }

        request* req;
        req = create_req(ReqMan);
        if (!req)
        {
            closesocket(clientSocket);
            continue;
        }

        req->numChld = numChld;
        req->numConn = ++allConn;
        req->numReq = 0;
        req->clientSocket = clientSocket;
        req->timeout = conf->TimeOut;

        req->remoteAddr[0] = '\0';
        req->remotePort[0] = '\0';
        getnameinfo((struct sockaddr*) & clientAddr,
            addrSize,
            req->remoteAddr,
            sizeof(req->remoteAddr),
            req->remotePort,
            sizeof(req->remotePort),
            NI_NUMERICHOST | NI_NUMERICSERV);

        ReqMan->push_req(req, 1);
    }

    int i = 0;
    n = ReqMan->get_num_thr();
    print_err("%d<%s:%d>  numThr=%d; allNumThr=%u; allConn=%u; num_req=%d\n", numChld,
        __func__, __LINE__, n, ReqMan->get_all_thr(), allConn, ReqMan->get_num_req());
    while (i < n)
    {
        request* req;
        req = create_req(ReqMan);
        if (!req)
        {
            break;
        }
        req->clientSocket = INVALID_SOCKET;
        ReqMan->push_req(req, 1);
        ++i;
    }

    ReqMan->close_manager();
    thrReqMan.join();
    close_queue();
    SendFile.join();
    CloseHandle(hParent);

    print_err("%d<%s:%d> *** Exit  ***\n", numChld, __func__, __LINE__);
    delete ReqMan;
    WSACleanup();
}
//======================================================================
request* create_req(RequestManager * ReqMan)
{
    request* req = NULL;
    while (1)
    {
        req = new(nothrow) request;
        if (!req)
        {
            print_err("%d<%s:%d> Error malloc(): errno=%d\n", ReqMan->get_num_chld(), __func__, __LINE__, errno);
            ReqMan->timedwait_close_req();
            continue;
        }
        break;
    }
    return req;
}
