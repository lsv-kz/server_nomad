#include "main.h"

using namespace std;

HANDLE hLogErrDup;

static SOCKET sockServer = -1;
static bool closeServer = false;
int read_conf_file(const char* path_conf);
//======================================================================
int main_proc(const char* name_proc);
void child_proc(SOCKET sock, int numChld, HANDLE, HANDLE, HANDLE);
//======================================================================
int main(int argc, char* argv[])
{
    if (read_conf_file(".") < 0)
    {
        cin.get();
        exit(1);
    }
    //------------------------------------------------------------------
    if (argc == 10)
    {
        setlocale(LC_CTYPE, "");
        if (!strcmp(argv[1], "child"))
        {
            int numChld;
            DWORD ParentID;
            SOCKET sockServ;
            HANDLE hIn, hOut, hReady;
            HANDLE hChildLog, hChildLogErr;

            stringstream ss;
            ss << argv[2] << ' ' << argv[3] << ' '
                << argv[4] << ' ' << argv[5] << ' '
                << argv[6] << ' ' << argv[7] << ' '
                << argv[8] << ' ' << argv[9];
            ss >> numChld;
            ss >> ParentID;
            ss >> sockServ;
            ss >> hIn;
            ss >> hOut;
            ss >> hReady;
            ss >> hChildLog;
            ss >> hChildLogErr;

            hLogErrDup = open_logfiles(hChildLog, hChildLogErr);

            SECURITY_ATTRIBUTES saAttr;
            saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
            saAttr.bInheritHandle = FALSE;
            saAttr.lpSecurityDescriptor = NULL;

            child_proc(sockServ, numChld, hIn, hOut, hReady);
            exit(0);
        }
        else
        {
            exit(1);
        }
    }
    else
    {
        printf(" LC_CTYPE: %s\n", setlocale(LC_CTYPE, ""));
        main_proc(argv[0]);
    }

    return 0;
}
//======================================================================
void create_logfiles(const wchar_t* log_dir, HANDLE* h, HANDLE* hErr);
void read_from_pipe(HANDLE);
void mprint_err(const char* format, ...);

mutex mtx_balancing;
condition_variable cond_wait;

int numConn = 0;
//======================================================================
int main_proc(const char* name_proc)
{
    DWORD pid = GetCurrentProcessId();
    HANDLE hPipeParent[6] = { NULL };
    HANDLE hLog, hLogErr;
    create_logfiles(conf->wLogDir.c_str(), &hLog, &hLogErr);

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = true;
    saAttr.lpSecurityDescriptor = NULL;
    //------------------------------------------------------------------
    sockServer = create_server_socket(conf);
    if (sockServer == INVALID_SOCKET)
    {
        cout << "<" << __LINE__ << "> server: failed to bind" << "\n";
        cin.get();
        exit(1);
    }

    if (conf->NumChld < 1)
    {
        cerr << "<" << __LINE__ << "> Error NumChld = " << conf->NumChld << "; [NumChld < 1]\n";
        exit(1);
    }
    cerr << " [" << get_time() << "] - server \"" << conf->ServerSoftware << "\" run\n"
        << "\n   pid = " << pid
        << "\n   ip = " << conf->host
        << "\n   port = " << conf->servPort
        << "\n   SockBufSize = " << conf->SOCK_BUFSIZE
        << "\n\n   NumChld = " << conf->NumChld
        << "\n   MaxThreads = " << conf->MaxThreads
        << "\n   MinThreads = " << conf->MinThreads
        << "\n\n   ListenBacklog = " << conf->ListenBacklog
        << "\n   MaxRequests = " << conf->MaxRequests
        << "\n   SizeQueue = " << conf->SizeQueue
        << "\n\n   KeepAlive " << conf->KeepAlive
        << "\n   TimeoutKeepAlive = " << conf->TimeoutKeepAlive
        << "\n   TimeOut = " << conf->TimeOut
        << "\n   TimeoutThreadCond = " << conf->TimeoutThreadCond
        << "\n   TimeOutCGI = " << conf->TimeOutCGI
        << "\n\n   php: " << conf->usePHP;
    wcerr << L"\n   path_php: " << conf->wPathPHP
        << L"\n   pyPath: " << conf->wPyPath
        << L"\n   PerlPath: " << conf->wPerlPath
        << L"\n   root_dir = " << conf->wRootDir
        << L"\n   cgi_dir = " << conf->wCgiDir
        << L"\n   log_dir = " << conf->wLogDir
        << L"\n   ShowMediaFiles = " << conf->ShowMediaFiles
        << L"\n   ClientMaxBodySize = " << conf->ClientMaxBodySize
        << L"\n\n";
    //------------------------------------------------------------------
    HANDLE ready_in = NULL;
    HANDLE ready_out = NULL;

    if (!CreatePipe(&ready_in, &ready_out, &saAttr, 0))
    {
        cerr << "<" << __LINE__ << "> Error: CreatePipe" << "\n";
        cin.get();
        exit(1);
    }

    if (!SetHandleInformation(ready_in, HANDLE_FLAG_INHERIT, 0))
    {
        cerr << "<" << __LINE__ << "> Error: SetHandleInformation" << "\n";
        cin.get();
        exit(1);
    }
    //------------------------------------------------------------------
    HANDLE to_in = NULL, to_out = NULL;
    HANDLE from_in = NULL, from_out = NULL;

    if (!CreatePipe(&to_in, &to_out, &saAttr, 0))
    {
        cerr << "<" << __LINE__ << "> Error: CreatePipe" << "\n";
        cin.get();
        exit(1);
    }

    if (!SetHandleInformation(to_out, HANDLE_FLAG_INHERIT, 0))
    {
        cerr << "<" << __LINE__ << "> Error: SetHandleInformation" << "\n";
        cin.get();
        exit(1);
    }

    if (!CreatePipe(&from_in, &from_out, &saAttr, 0))
    {
        cerr << "<" << __LINE__ << "> Error: CreatePipe" << "\n";
        cin.get();
        exit(1);
    }

    if (!SetHandleInformation(from_in, HANDLE_FLAG_INHERIT, 0))
    {
        cerr << "<" << __LINE__ << "> Error: SetHandleInformation" << "\n";
        cin.get();
        exit(1);
    }
    //------------------------------------------------------------------
    int numChld = 0;
    while (numChld < conf->NumChld)
    {
        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(STARTUPINFO));
        si.cb = sizeof(STARTUPINFO);
        si.dwFlags |= STARTF_USESTDHANDLES;

        stringstream ss;
        ss << name_proc << " child " << numChld << ' ' << pid << ' '
            << sockServer << ' ' << to_in << ' ' << from_out << ' ' << ready_out << ' '
            << hLog << ' ' << hLogErr;

        cerr << name_proc << " child " << numChld << "\n";
        bool bSuccess = CreateProcessA(NULL, (char*)ss.str().c_str(), NULL, NULL, true, 0, NULL, NULL, &si, &pi);
        if (!bSuccess)
        {
            DWORD err = GetLastError();
            mprint_err("<%s:%d> Error CreateProcessA(): %s\n error=%lu\n", __func__, __LINE__, ss.str().c_str(), err);
            exit(1);
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        ++numChld;
    }

    CloseHandle(ready_out);
    CloseHandle(to_in);
    CloseHandle(from_out);
    //------------------------------------------------------------------
    if (_wchdir(conf->wRootDir.c_str()))
    {
        wcerr << "!!! Error chdir(\"" << conf->wRootDir << "\") : " << errno << "\n";
        cin.get();
        exit(1);
    }
    //------------------------------------------------------------------
    thread ReadPipe;
    try
    {
        ReadPipe = thread(read_from_pipe, ready_in);
    }
    catch (...)
    {
        mprint_err("%d<%s:%d> Error create thread(read_from_pipe)\n", numChld, __func__, __LINE__);
        exit(1);
    }

    int num_chld = 0;
    fd_set rdfds;
    FD_ZERO(&rdfds);
    while (1)
    {
        FD_SET(sockServer, &rdfds);

        {
            unique_lock<mutex> lk(mtx_balancing);
            while (numConn <= 0)
            {
                cond_wait.wait(lk);
            }
        }
 
        int ret = select(0, &rdfds, NULL, NULL, NULL);
        if ((ret == SOCKET_ERROR) || closeServer)
        {
            if (closeServer)
                mprint_err("<%s:%d> closeServer=%u\n", __func__, __LINE__, closeServer);
            else
                mprint_err("<%s:%d> Error select(): %d", __func__, __LINE__, WSAGetLastError());
            break;
        }

        if (FD_ISSET(sockServer, &rdfds))
        {
            unsigned char ch = 1;
            DWORD wr;
            bool res = WriteFile(to_out, &ch, 1, &wr, NULL);
            if (!res)
            {
                int err = GetLastError();
                mprint_err("<%s:%d> Error WriteFile(): %d\n", __func__, __LINE__, err);
                break;
            }

            res = ReadFile(from_in, &ch, sizeof(ch), &wr, NULL);
            if (!res || wr == 0)
            {
                int err = GetLastError();
                mprint_err("<%s:%d> Error ReadFile(): %d\n", __func__, __LINE__, err);
                break;
            }

            if (ch != 0x02)
            {
                mprint_err("<%s:%d>  Error ch = 0x%hhx\n", __func__, __LINE__, ch);
            }

            mtx_balancing.lock();
            --numConn;
            mtx_balancing.unlock();
        }
    }

    for (int i = 0; (i < conf->NumChld) && (!closeServer); ++i)
    {
        DWORD wr;
        unsigned char ch = 0x80;
        bool res = WriteFile(to_out, &ch, 1, &wr, NULL);
        if (!res)
        {
            int err = GetLastError();
            mprint_err("<%s:%d> Error WriteFile(): %d\n", __func__, __LINE__, err);
            break;
        }

        res = ReadFile(from_in, &ch, sizeof(ch), &wr, NULL);
        if (!res || wr == 0)
        {
            int err = GetLastError();
            mprint_err("<%s:%d> Error ReadFile(): %d\n", __func__, __LINE__, err);
            break;
        }
    }

    CloseHandle(ready_in);
    CloseHandle(to_out);
    CloseHandle(from_in);

    ReadPipe.join();

    return 0;
}
//======================================================================
void read_from_pipe(HANDLE ready_in)
{
    while (1)
    {
        DWORD rd;
        unsigned char ch;
        bool res = ReadFile(ready_in, &ch, 1, &rd, NULL);
        if (!res || rd == 0)
        {
            DWORD err = GetLastError();
            mprint_err("<%s:%d> Error ReadFile(): %lu\n", __func__, __LINE__, err);
            break;
        }

        if (ch >= conf->NumChld)
        {
            mprint_err("<%s:%d> ch=%d\n", __func__, __LINE__, (int)ch);
            break;
        }
    mtx_balancing.lock();
        ++numConn;
    mtx_balancing.unlock();
        cond_wait.notify_one();
    }
    CloseHandle(ready_in);
    mprint_err("<%s:%d> Exit thread: read_from_pipe\n", __func__, __LINE__);
}

