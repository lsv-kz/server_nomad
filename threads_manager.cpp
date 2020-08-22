#include "main.h"

using namespace std;

Connect* create_req(RequestManager* ReqMan);
//======================================================================
RequestManager::RequestManager(int n)
{
    list_begin = list_end = NULL;
    len_qu = stop_manager = all_thr = 0;
    count_thr = count_conn = num_wait_thr = 0;
    numChld = n;
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
int RequestManager::push_req(Connect* req, int inc)
{
    int ret;
mtx_thr.lock();
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
        ++count_conn;
    ret = ++len_qu;
mtx_thr.unlock();

    cond_push.notify_one();

    return ret;
}
//----------------------------------------------------------------------
Connect* RequestManager::pop_req()
{
    Connect* req;
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
    if (num_wait_thr == 0)
    {
        need_create_thr = 1;
        cond_new_thr.notify_one();
    }

    return req;
}
//----------------------------------------------------------------------
int RequestManager::end_thr(int* nthr, int* nreq)
{
    int ret = 0;
 mtx_thr.lock();
    if ((count_thr > conf->MinThreads) && (len_qu <= num_wait_thr))
    {
        --count_thr;
        ret = EXIT_THR;
    }

    *nthr = count_thr;
    *nreq = count_conn;
 mtx_thr.unlock();
    if (ret)
        cond_exit_thr.notify_all();
    return ret;
}
//--------------------------------------------
void RequestManager::close_manager()
{
    stop_manager = 1;
    cond_new_thr.notify_one();
}
//----------------------------------------------------------------------
void RequestManager::end_response(Connect* req)
{
    if ((req->connKeepAlive == 0)  || req->err < 0)
    {
        //   if (req->err != -1)
        print_log(req);
        //----------------- close connect ------------------------------
        shutdown(req->clientSocket, SD_BOTH);
        closesocket(req->clientSocket);
        delete req;
     mtx_thr.lock();
        --count_conn;
     mtx_thr.unlock();
        cond_close_conn.notify_all();
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
    while (1)
    {
        unique_lock<mutex> lk(mtx_thr);
        while (((need_create_thr == 0) || (num_wait_thr > 0)) && !stop_manager)
        {
            cond_new_thr.wait(lk);
        }

        if (stop_manager)
            return num_wait_thr;

        while ((count_thr >= conf->MaxThreads) && !stop_manager)
        {
            cond_exit_thr.wait(lk);
        }

        if (stop_manager)
            return num_wait_thr;

        if ((need_create_thr > 0) && (num_wait_thr <= 0))
        {
            need_create_thr = 0;
            *n = count_thr;
            break;
        }
    }

    return stop_manager;
}
//----------------------------------------------------------------------
int RequestManager::check_num_conn()
{
unique_lock<mutex> lk(mtx_thr);
    while (count_conn >= conf->MaxRequests)
        cond_close_conn.wait(lk);
    return 0;
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
SOCKET Connect::serverSocket;
//======================================================================
void child_proc(SOCKET sockServer, int numChld, HANDLE hExit_out)
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

    Connect::serverSocket = sockServer;
    //------------------------------------------------------------------
    ReqMan = new(nothrow) RequestManager(numChld);
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
        socklen_t addrSize;
        struct sockaddr_storage clientAddr;

        ReqMan->check_num_conn();

        addrSize = sizeof(struct sockaddr_storage);
        SOCKET clientSocket = accept(sockServer, (struct sockaddr*) & clientAddr, &addrSize);
        if (clientSocket == INVALID_SOCKET)
        {
            int err = ErrorStrSock(__func__, __LINE__, "Error accept()");
            if (err == WSAEMFILE)
            {
                print_err("%d<%s:%d> Error accept(): WSAEMFILE\n", numChld, __func__, __LINE__);
                continue;
            }
            else
            {
                print_err("%d<%s:%d> Error accept(): %d\n", numChld, __func__, __LINE__, err);
                break;
            }
        }

        Connect* req;
        req = create_req(ReqMan);
        if (!req)
        {
            closesocket(clientSocket);
            continue;
        }

        u_long iMode = 1;
        if (ioctlsocket(clientSocket, FIONBIO, &iMode) == SOCKET_ERROR)
        {
            print_err("<%s:%d> Error ioctlsocket(): %d\n", __func__, __LINE__, WSAGetLastError());
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

    ReqMan->close_manager();
    thrReqMan.join();

    int i = 0;
    n = ReqMan->get_num_thr();
    print_err("%d<%s:%d>  numThr=%d; allConn=%u\n", numChld,
        __func__, __LINE__, n, allConn);
    while (i < n)
    {
        Connect* req;
        req = create_req(ReqMan);
        if (!req)
        {
            break;
        }
        req->clientSocket = INVALID_SOCKET;
        ReqMan->push_req(req, 1);
        ++i;
    }

    
    close_queue();
    SendFile.join();

    unsigned char ch = (unsigned char)numChld;
    DWORD rd;
    bool res = WriteFile(hExit_out, &ch, 1, &rd, NULL);
    if (!res)
    {
        PrintError(__func__, __LINE__, "Error WriteFile()");
    }
    CloseHandle(hExit_out);

    print_err("%d<%s:%d> *** Exit  ***\n", numChld, __func__, __LINE__);
    delete ReqMan;
    WSACleanup();
}
//======================================================================
Connect* create_req(RequestManager * ReqMan)
{
    Connect* req = NULL;

    req = new(nothrow) Connect;
    if (!req)
    {
        print_err("%d<%s:%d> Error malloc()\n", ReqMan->get_num_chld(), __func__, __LINE__);
    }
    return req;
}
