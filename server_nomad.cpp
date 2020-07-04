#include "main.h"

using namespace std;

HANDLE hLogErrDup;

static SOCKET sockServer = -1;
static bool closeServer = false;
int read_conf_file(const char* path_conf);

char pipeName[40] = "\\\\.\\pipe\\start-";
//======================================================================
BOOL WINAPI CtrlHandlerChild(DWORD fdwCtrlType)
{
    if (fdwCtrlType == CTRL_C_EVENT)
    {
        return TRUE;
    }
    return FALSE;
}
//======================================================================
int main_proc(const char* name_proc);
void child_proc(SOCKET sock, int numChld, HANDLE, HANDLE);
//======================================================================
int main(int argc, char* argv[])
{
    read_conf_file(".");
    //------------------------------------------------------------------
    if (argc == 8)
    {
        setlocale(LC_CTYPE, "");
        if (!strcmp(argv[1], "child"))
        {
            if (!SetConsoleCtrlHandler(CtrlHandlerChild, TRUE))
            {
                printf("Error: SetConsoleCtrlHandler()\n");
                return 1;
            }

            int numChld;
            DWORD ParentID;
            SOCKET sockServ;
            HANDLE hPipeChld, hCloseReq;
            HANDLE hChildLog, hChildLogErr;

            stringstream ss;
            ss << argv[2] << ' ' << argv[3] << ' '
                << argv[4] << ' ' << argv[5] << ' '
                << argv[6] << ' ' << argv[7];
            ss >> numChld;
            ss >> ParentID;
            ss >> sockServ;
            ss >> hCloseReq;
            ss >> hChildLog;
            ss >> hChildLogErr;

            hLogErrDup = open_logfiles(hChildLog, hChildLogErr);

            SECURITY_ATTRIBUTES saAttr;
            saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
            saAttr.bInheritHandle = FALSE;
            saAttr.lpSecurityDescriptor = NULL;

            size_t n = strlen(pipeName);
            snprintf(pipeName + n, sizeof(pipeName) - n, "%lu-%d", ParentID, numChld);
            hPipeChld = CreateFileA(
                pipeName,
                GENERIC_READ | GENERIC_WRITE,
                0,
                &saAttr,
                OPEN_EXISTING,
                0,
                NULL);
            if (hPipeChld == INVALID_HANDLE_VALUE)
            {
                print_err("<%d> Error CreateFile, GLE=%lu; [%s]\n", __LINE__, GetLastError(), pipeName);
                cin.get();
                exit(1);
            }

            child_proc(sockServ, numChld, hPipeChld, hCloseReq);
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
void read_from_pipe(HANDLE close_in);
void mprint_err(const char* format, ...);

mutex mtx_balancing;
int numConn[6] = { 0, 0, 0, 0, 0, 0 };
//======================================================================
int main_proc(const char* name_proc)
{
    DWORD pid = GetCurrentProcessId();
    HANDLE hPipeParent[6] = { NULL };
    HANDLE hLog, hLogErr;
    create_logfiles(conf->wLogDir.c_str(), &hLog, &hLogErr);

    size_t len = strlen(pipeName);
    snprintf(pipeName + len, sizeof(pipeName) - len, "%lu-", pid);

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

    if ((conf->NumChld < 1) || (conf->NumChld > 6))
    {
        cerr << "<" << __LINE__ << "> Error NumChld = " << conf->NumChld << "; [1 < NumChld <= 6]\n";
        exit(1);
    }
    cerr << " [" << get_time() << "] - server \"" << conf->ServerSoftware << "\" run\n"
        << "\n   pid = " << pid
        << "\n   ip = " << conf->host
        << "\n   port = " << conf->servPort
        << "\n   SockBufSize = " << conf->SOCK_BUFSIZE
        << "\n   NumChld = " << conf->NumChld
        << "\n\n   ListenBacklog = " << conf->ListenBacklog
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
    HANDLE close_in = NULL;
    HANDLE close_out = NULL;

    if (!CreatePipe(&close_in, &close_out, &saAttr, 0))
    {
        cerr << "<" << __LINE__ << "> Error: CreatePipe" << "\n";
        cin.get();
        exit(1);
    }

    if (!SetHandleInformation(close_in, HANDLE_FLAG_INHERIT, 0))
    {
        cerr << "<" << __LINE__ << "> Error: SetHandleInformation" << "\n";
        cin.get();
        exit(1);
    }
    //------------------------------------------------------------------
    int numChld = 0;
    const int pipeBufSize = 8;
    len = strlen(pipeName);
    while (numChld < conf->NumChld)
    {
        snprintf(pipeName + len, sizeof(pipeName) - len, "%d", numChld);

        hPipeParent[numChld] = CreateNamedPipeA(
            pipeName,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE |
            PIPE_READMODE_BYTE |
            PIPE_WAIT,
            1,
            pipeBufSize,
            pipeBufSize,
            5000,
            NULL);
        if (hPipeParent[numChld] == INVALID_HANDLE_VALUE)
        {
            printf("<%d> CreateNamedPipe failed, GLE=%lu\n", __LINE__, GetLastError());
            cin.get();
            return -1;
        }

        if (!SetHandleInformation(hPipeParent[numChld], HANDLE_FLAG_INHERIT, 0))
        {
            printf("<%d> Error SetHandleInformation, GLE=%lu\n", __LINE__, GetLastError());
            cin.get();
            exit(1);
        }

        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(STARTUPINFO));
        si.cb = sizeof(STARTUPINFO);
        si.dwFlags |= STARTF_USESTDHANDLES;

        stringstream ss;
        ss << name_proc << " child " << numChld << ' ' << pid << ' '
            << sockServer << ' ' << close_out << ' ' << hLog << ' ' << hLogErr;

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
        ConnectNamedPipe(hPipeParent[numChld], NULL);
        ++numChld;
    }

    CloseHandle(close_out);
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
        ReadPipe = thread(read_from_pipe, close_in);
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
            int minConn = numConn[0];
            int i = 0;
            num_chld = 0;
            while (i < conf->NumChld)
            {
                lock_guard<mutex> lk(mtx_balancing);
                if (numConn[i] < minConn)
                {
                    minConn = numConn[i];
                    num_chld = i;
                }
                ++i;
            }

            unsigned char ch = num_chld;
            DWORD wr;
            bool res = WriteFile(hPipeParent[num_chld], &ch, 1, &wr, NULL);
            if (!res)
            {
                int err = GetLastError();
                mprint_err("<%s:%d> Error WriteFile(): %d\n", __func__, __LINE__, err);
                break;
            }

            res = ReadFile(hPipeParent[num_chld], &ch, sizeof(ch), &wr, NULL);
            if (!res || wr == 0)
            {
                int err = GetLastError();
                mprint_err("<%s:%d> Error ReadFile(): %d\n", __func__, __LINE__, err);
                break;
            }

            if (ch == num_chld)
            {
                mtx_balancing.lock();
                ++numConn[num_chld];
                mtx_balancing.unlock();
            }
        }
    }

    for (int i = 0; (i < conf->NumChld) && (!closeServer); ++i)
    {
        DWORD wr;
        unsigned char ch = 0x80;
        bool res = WriteFile(hPipeParent[i], &ch, 1, &wr, NULL);
        if (!res)
        {
            int err = GetLastError();
            mprint_err("<%s:%d> Error WriteFile(): %d\n", __func__, __LINE__, err);
            break;
        }

        res = ReadFile(hPipeParent[i], &ch, sizeof(ch), &wr, NULL);
        if (!res || wr == 0)
        {
            int err = GetLastError();
            mprint_err("<%s:%d> Error ReadFile(): %d\n", __func__, __LINE__, err);
            break;
        }

        CloseHandle(hPipeParent[i]);
    }

    CloseHandle(close_in);
    ReadPipe.join();

    return 0;
}
//======================================================================
void read_from_pipe(HANDLE close_in)
{
    while (1)
    {
        DWORD rd;
        unsigned char ch;
        bool res = ReadFile(close_in, &ch, 1, &rd, NULL);
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
        --numConn[ch];
        mtx_balancing.unlock();
    }
    CloseHandle(close_in);
    mprint_err("<%s:%d> Exit thread: read_from_pipe\n", __func__, __LINE__);
}

