#include "main.h"

using namespace std;

request* create_req(RequestManager* ReqMan);
//======================================================================
RequestManager::RequestManager(int n, HANDLE pipe_out)
{
    count_push = count_pop = len_qu = stop_manager = all_thr = 0;
    count_thr = count_req = num_wait_thr = num_create_thr = 0;
    numChld = n;
    hClose_out = pipe_out;

    quReq = new(nothrow) request * [conf->SizeQueue * sizeof(request*)];
    if (!quReq)
    {
        print_err("<%s:%d> Error new()\n", __func__, __LINE__);
        exit(1);
    }
    memset(quReq, 0, conf->SizeQueue * sizeof(request*));
}
//----------------------------------------------------------------------
RequestManager::~RequestManager()
{
    delete[] quReq;
}
//----------------------------------------------------------------------
int RequestManager::get_num_chld(void)
{
    return numChld;
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
int RequestManager::get_num_thr(void)
{
    mtx_thr.lock();
    int ret = count_thr;
    mtx_thr.unlock();
    return ret;
}
//----------------------------------------------------------------------
int RequestManager::get_all_req_thr(void)
{
    return all_thr;
}
//----------------------------------------------------------------------
int RequestManager::start_thr(void)
{
    mtx_thr.lock();
    int ret = ++count_thr;
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
int RequestManager::start_req(void)
{
    mtx_thr.lock();
    int ret = ++count_req;
    mtx_thr.unlock();
    //	cond_start_req.notify_one();
    return ret;
}
//----------------------------------------------------------------------
void RequestManager::wait_close_req(int n)
{
    unique_lock<mutex> lk(mtx_close_req);
    while (count_req >= n)
    {
        cond_close_req.wait(lk);
    }
}
//----------------------------------------------------------------------
void RequestManager::timedwait_close_req(void)
{
    unique_lock<mutex> lk(mtx_close_req);
    cond_close_req.wait_for(lk, chrono::milliseconds(1000 * conf->TimeoutThreadCond));
}
//----------------------------------------------------------------------
int RequestManager::push_req(request* req)
{
    int ret, n_wait_thr;
    {
        unique_lock<mutex> lk(mtx_qu);
        while (quReq[count_push])
        {
            cond_pop.wait(lk);
        }

        quReq[count_push] = req;
        ret = ++len_qu;
        n_wait_thr = num_wait_thr;
    }
    ++count_push;
    if (count_push >= conf->SizeQueue) count_push = 0;

    cond_push.notify_one();

    if (n_wait_thr < ret)
    {
        mtx_thr.lock();
        ++num_create_thr;
        mtx_thr.unlock();
        cond_start_req.notify_one();
    }

    return ret;
}
//----------------------------------------------------------------------
request* RequestManager::pop_req()
{
    request* req;
    {
        unique_lock<mutex> lk(mtx_qu);

        ++num_wait_thr;
        while (quReq[count_pop] == NULL)
        {
            cond_push.wait(lk);
        }

        --num_wait_thr;
        req = quReq[count_pop];
        quReq[count_pop] = NULL;

        --len_qu;
    }

    ++count_pop;
    if (count_pop >= conf->SizeQueue) count_pop = 0;

    cond_pop.notify_one();
    return req;
}
//----------------------------------------------------------------------
int RequestManager::end_req(int* nthr, int* nreq)
{
    int ret = 0;
    mtx_thr.lock();
    if (num_wait_thr < num_create_thr)
    {
        --num_create_thr;
    }
    else if (count_thr > conf->MinThreads)
    {
        --count_thr;
        ret = EXIT_THR;
    }

    *nthr = count_thr;
    *nreq = count_req;
    mtx_thr.unlock();
    if (ret)
        cond_exit_thr.notify_one();
    return ret;
}
//----------------------------------------------------------------------
int RequestManager::wait_new_req(void)
{
    unique_lock<mutex> lk(mtx_thr);

    while ((num_create_thr <= 0) && !stop_manager)
    {
        cond_start_req.wait(lk);
    }
    --num_create_thr;
    return stop_manager;
}
//----------------------------------------------------------------------
int RequestManager::check_num_thr(int* nthr)
{
    unique_lock<mutex> lk(mtx_thr);
    while ((count_thr >= conf->MaxThreads) && !stop_manager)
    {
        cond_exit_thr.wait(lk);
    }
    return stop_manager;
}
//----------------------------------------------------------------------
int RequestManager::get_len_qu(void)
{
    mtx_qu.lock();
    int ret = len_qu;
    mtx_qu.unlock();
    return ret;
}
//--------------------------------------------
void RequestManager::close_manager()
{
    stop_manager = 1;
    cond_start_req.notify_one();
}
//----------------------------------------------------------------------
void RequestManager::close_connect(request * req)
{
    if (req->err != -1)
        print_log(req);
    shutdown(req->clientSocket, SD_BOTH);
    closesocket(req->clientSocket);
    delete req;
    //----------- close_req(); ---------------
    mtx_thr.lock();
    --count_req;
    mtx_thr.unlock();
    cond_close_req.notify_one();
    unsigned char ch = (unsigned char)numChld;
    DWORD rd;
    bool res = WriteFile(hClose_out, &ch, 1, &rd, NULL);
    if (!res)
    {
        PrintError(__func__, __LINE__, "Error WriteFile()");
        exit(1);
    }
}
//----------------------------------------------------------------------
void RequestManager::close_response(request * req)
{
    if (req->connKeepAlive == 0) // || req->err < 0)
        close_connect(req);
    else
    {
        if (req->err != -1)
            print_log(req);
        ++req->numReq;
        push_req(req);
    }
}
//======================================================================
void thread_req_manager(int numProc, RequestManager * ReqMan)
{
    int num_thr;
    thread thr;

    while (1)
    {
        if (ReqMan->wait_new_req()) break;
        if (ReqMan->check_num_thr(&num_thr)) break;

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

        num_thr = ReqMan->start_thr();
        ReqMan->inc_all_thr();
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
        print_err("%d<%s:%d> Error create thread(send_file_): errno=%d \n", numChld, __func__, __LINE__, errno);
        exit(errno);
    }

    SendFile.detach();
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
            print_err("%d<%s:%d> Error create thread: errno=%d\n", numChld, __func__, __LINE__);
            exit(errno);
        }
        ++allNumThr;
        ReqMan->start_thr();
        ReqMan->inc_all_thr();
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
        print_err("<%s:%d> Error create thread %d: errno=%d\n", __func__,
            __LINE__, allNumThr, errno);
        exit(errno);
    }

    while (1)
    {
        unsigned char ch;
        socklen_t addrSize;
        struct sockaddr_storage clientAddr;
        ReqMan->wait_close_req(470);
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
            ch = 0x80;
            res = WriteFile(hParent, &ch, 1, &rd, NULL);
            if (!res)
            {
                PrintError(__func__, __LINE__, "Error WriteFile()");
                break;
            }

            int err = ErrorStrSock(__func__, __LINE__, "Error accept()");
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

        ReqMan->push_req(req);
        ReqMan->start_req();
    }

    int i = 0;
    n = ReqMan->get_num_thr();
    print_err("%d<%s:%d>  numThr=%d; allNumThr=%u; allConn=%u; num_req=%d\n", numChld,
        __func__, __LINE__, n, ReqMan->get_all_req_thr(), allConn, ReqMan->get_num_req());
    while (i < n)
    {
        request* req;
        req = create_req(ReqMan);
        if (!req)
        {
            break;
        }
        req->clientSocket = INVALID_SOCKET;
        ReqMan->push_req(req);
        ++i;
    }

    ReqMan->close_manager();
    thrReqMan.join();
    close_conv();
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
