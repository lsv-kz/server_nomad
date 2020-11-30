#include "chunk.h"

int cgi_chunk(Connect* req, PIPENAMED* Pipe, int maxRd);
//======================================================================
class CreateEnv
{
private:
    size_t sizeBufEnv;
    size_t i;
    char* bufEnv;

public:
    size_t add(const char* name, const char* val)
    {
        ostringstream ss;
        if (name)
            ss << name << '=';
        else
            return 0;

        if (val)
            ss << val;

        size_t len = ss.str().size();
        if ((i + len + 2) > sizeBufEnv)
        {
            int sizeTmp = sizeBufEnv + len + 128;
            char* tmp = new(nothrow) char[sizeTmp];
            if (!tmp)
                return 0;
            sizeBufEnv = sizeTmp;
            memcpy(tmp, bufEnv, i);
            delete[] bufEnv;
            bufEnv = tmp;
        }
        memcpy(bufEnv + i, ss.str().c_str(), len);
        i += len;
        bufEnv[i++] = '\0';
        bufEnv[i] = '\0';
        return len + 2;
    }
    //------------------------------------------------------------------
    char* get() { return bufEnv; }
    //------------------------------------------------------------------
    size_t size() { return i + 1; }
    //------------------------------------------------------------------
    CreateEnv()
    {
        i = 0;
        sizeBufEnv = 1024;
        bufEnv = new(nothrow) char[sizeBufEnv];
        if (!bufEnv)
            sizeBufEnv = 0;
    }
    //------------------------------------------------------------------
    ~CreateEnv()
    {
        if (bufEnv) delete[] bufEnv;
    }
};
//======================================================================
wstring cgi_script_file(const wstring& name)
{
    const wchar_t* p;

    if ((p = wcschr(name.c_str() + 1, '/')))
    {
        return p;
    }
    return name;
}
//======================================================================
int cgi(Connect* req)
{
    int retExit = -1;
    size_t len;
    long long wr_bytes;
    struct _stat st;
    BOOL bSuccess;
    string stmp;

    char pipeName[40] = "\\\\.\\pipe\\cgi";
    const DWORD PIPE_BUFSIZE = 1024;

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE childPipe;
    PIPENAMED Pipe;
    ZeroMemory(&Pipe.oOverlap, sizeof(Pipe.oOverlap));

    wstring commandLine;
    CreateEnv env;
    stringstream ss;

    wstring wPath;

    if (req->resp.scriptType == cgi_ex)
    {
        wPath += conf->wCgiDir;
        wPath += cgi_script_file(req->wScriptName);
    }
    else if (req->resp.scriptType == php_cgi)
    {
        wPath += conf->wRootDir;
        wPath += req->wScriptName;
    }

    if (_wstat(wPath.c_str(), &st) == -1)
    {
        utf16_to_utf8(stmp, wPath);
        print_err(req, "<%s:%d> script (%s) not found: errno=%d\n", __func__,
            __LINE__, stmp.c_str(), errno);
        retExit = -RS404;
        goto errExit;
    }
    //--------------------- set environment ----------------------------
     {
        const int size = 4096;
        char tmpBuf[size];
        if (GetWindowsDirectory(tmpBuf, size))
        {
            env.add("SYSTEMROOT", tmpBuf);
        }
        else
        {
            print_err(req, "<%s:%d> Error getenv_s()\n", __func__, __LINE__);
            retExit = -RS500;
            goto errExit;
        }
    }
  
    {
        const int size = 4096;
        char tmpBuf[size];
        if (ExpandEnvironmentStringsA("PATH=%PATH%", tmpBuf, size))
        {
            env.add("PATH", tmpBuf);
        }
        else
        {
            print_err(req, "<%s:%d> Error getenv_s()\n", __func__, __LINE__);
            retExit = -RS500;
            goto errExit;
        }
    }
   
    if (req->reqMethod == M_POST)
    {
        if (req->req_hdrs.iReqContentLength >= 0)
            env.add("CONTENT_LENGTH", req->req_hdrs.Value[req->req_hdrs.iReqContentLength]);
        else
        {
            //      print_err(req, "<%s:%d> 411 Length Required\n", __func__, __LINE__);
            retExit = -RS411;
            goto errExit;
        }

        if (req->req_hdrs.reqContentLength > conf->ClientMaxBodySize)
        {
            req->connKeepAlive = 0;
            retExit = -RS413;
            goto errExit;
        }

        if (req->req_hdrs.iReqContentType >= 0)
            env.add("CONTENT_TYPE", req->req_hdrs.Value[req->req_hdrs.iReqContentType]);
        else
        {
            print_err(req, "<%s:%d> Content-Type \?\n", __func__, __LINE__);
            retExit = -RS400;
            goto errExit;
        }
    }

    if (req->resp.scriptType == php_cgi)
        env.add("REDIRECT_STATUS", "true");
    if (req->req_hdrs.iUserAgent >= 0)
        env.add("HTTP_USER_AGENT", req->req_hdrs.Value[req->req_hdrs.iUserAgent]);
    if (req->req_hdrs.iReferer >= 0)
        env.add("HTTP_REFERER", req->req_hdrs.Value[req->req_hdrs.iReferer]);
    if (req->req_hdrs.iHost >= 0)
        env.add("HTTP_HOST", req->req_hdrs.Value[req->req_hdrs.iHost]);

    env.add("SERVER_PORT", conf->servPort.c_str());

    env.add("SERVER_SOFTWARE", conf->ServerSoftware.c_str());
    env.add("GATEWAY_INTERFACE", "CGI/1.1");
    env.add("REQUEST_METHOD", get_str_method(req->reqMethod));
    env.add("REMOTE_HOST", req->remoteAddr);
    env.add("SERVER_PROTOCOL", get_str_http_prot(req->httpProt));

    utf16_to_mbs(stmp, conf->wRootDir.c_str());
    env.add("DOCUMENT_ROOT", stmp.c_str());

    utf16_to_mbs(stmp, req->wDecodeUri.c_str());
    env.add("REQUEST_URI", stmp.c_str());

    utf16_to_mbs(stmp, wPath.c_str());
    env.add("SCRIPT_FILENAME", stmp.c_str());

    utf16_to_mbs(stmp, req->wDecodeUri.c_str());
    env.add("SCRIPT_NAME", stmp.c_str());

    env.add("REMOTE_ADDR", req->remoteAddr);
    env.add("REMOTE_PORT", req->remotePort);
    env.add("QUERY_STRING", req->sReqParam);
    //------------------------------------------------------------------
    if (req->resp.scriptType == php_cgi)
    {
        commandLine = conf->wPathPHP;
    }
    else if (req->resp.scriptType == cgi_ex)
    {
        if (wcsstr(req->wScriptName, L".pl"))
        {
            commandLine = conf->wPerlPath;
            commandLine += L' ';
            commandLine += wPath;
        }
        else if (wcsstr(req->wScriptName, L".py"))
        {
            commandLine = conf->wPyPath;
            commandLine += L' ';
            commandLine += wPath;
        }
        else
        {
            commandLine = wPath;
        }
    }
    else
    {
        print_err(req, "<%s:%d> Error CommandLine CreateProcess()\n", __func__, __LINE__);
        retExit = -RS500;
        goto errExit;
    }
    //------------------------------------------------------------------
    Pipe.hEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    if (Pipe.hEvent == NULL)
    {
        print_err(req, "<%s:%d> CreateEvent failed with %lu\n", __func__, __LINE__, GetLastError());
        retExit = -RS500;
        goto errExit;
    }
    Pipe.oOverlap.hEvent = Pipe.hEvent;
    //------------------------------------------------------------------

    len = strlen(pipeName);
    snprintf(pipeName + len, sizeof(pipeName) - len, "%d%d", req->numChld, req->numConn);
    Pipe.parentPipe = CreateNamedPipeA(
        pipeName,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE |
        PIPE_READMODE_BYTE |
        PIPE_WAIT,
        1,
        PIPE_BUFSIZE,
        PIPE_BUFSIZE,
        5000,
        NULL);
    if (Pipe.parentPipe == INVALID_HANDLE_VALUE)
    {
        print_err(req, "<%s:%d> CreateNamedPipe failed, GLE=%lu\n", __func__, __LINE__, GetLastError());
        retExit = -RS500;

        CloseHandle(Pipe.hEvent);

        goto errExit;
    }

    if (!SetHandleInformation(Pipe.parentPipe, HANDLE_FLAG_INHERIT, 0))
    {
        print_err(req, "<%s:%d> Error SetHandleInformation, GLE=%lu\n", __func__, __LINE__, GetLastError());
        retExit = -RS500;

        CloseHandle(Pipe.hEvent);

        DisconnectNamedPipe(Pipe.parentPipe);
        CloseHandle(Pipe.parentPipe);

        goto errExit;
    }
    //------------------------------------------------------------------
    childPipe = CreateFileA(
        pipeName,
        GENERIC_WRITE | GENERIC_READ,
        0,
        &saAttr,
        OPEN_EXISTING,
        0,
        NULL);
    if (childPipe == INVALID_HANDLE_VALUE)
    {
        print_err(req, "<%s:%d> Error CreateFile, GLE=%lu\n", __func__, __LINE__, GetLastError());
        retExit = -RS500;

        CloseHandle(Pipe.hEvent);

        DisconnectNamedPipe(Pipe.parentPipe);
        CloseHandle(Pipe.parentPipe);

        goto errExit;
    }

    ConnectNamedPipe(Pipe.parentPipe, &Pipe.oOverlap);

    PROCESS_INFORMATION pi;
    STARTUPINFOW si;
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&si, sizeof(STARTUPINFO));

    si.cb = sizeof(STARTUPINFO);
    si.hStdOutput = childPipe;
    si.hStdInput = childPipe;
    si.hStdError = GetHandleLogErr();
    si.dwFlags |= STARTF_USESTDHANDLES;

    bSuccess = CreateProcessW(NULL, (wchar_t*)commandLine.c_str(), NULL, NULL, TRUE, 0, env.get(), NULL, &si, &pi);
    CloseHandle(childPipe);
    if (!bSuccess)
    {
        utf16_to_utf8(stmp, commandLine);
        print_err(req, "<%s:%d> Error CreateProcessW(%s)\n", __func__, __LINE__, stmp.c_str());
        PrintError(__func__, __LINE__, "Error CreateProcessW()");
        retExit = -RS500;

        CloseHandle(Pipe.hEvent);

        DisconnectNamedPipe(Pipe.parentPipe);
        CloseHandle(Pipe.parentPipe);

        goto errExit;
    }
    else
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    //------------ write to script ------------
    if (req->reqMethod == M_POST)
    {
        if (req->tail)
        {
            wr_bytes = WriteToPipe(&Pipe, req->tail, req->lenTail, PIPE_BUFSIZE, conf->TimeOutCGI);
            if (wr_bytes < 0)
            {
                print_err(req, "%d<%s:%d> Error write_to_script()=%d\n", __func__, __LINE__, wr_bytes);
                retExit = -RS500;

                CloseHandle(Pipe.hEvent);

                DisconnectNamedPipe(Pipe.parentPipe);
                CloseHandle(Pipe.parentPipe);

                goto errExit;
            }
            req->req_hdrs.reqContentLength -= wr_bytes;
        }

        if (req->req_hdrs.reqContentLength > 0)
        {
            wr_bytes = client_to_script(req->clientSocket, &Pipe, req->req_hdrs.reqContentLength, PIPE_BUFSIZE, conf->TimeOutCGI);
            if (wr_bytes < 0)
            {
                print_err(req, "<%s:%d> Error client_to_script()\n", __func__, __LINE__);
                retExit = -RS500;

                CloseHandle(Pipe.hEvent);

                DisconnectNamedPipe(Pipe.parentPipe);
                CloseHandle(Pipe.parentPipe);

                goto errExit;
            }
            else
                req->req_hdrs.reqContentLength -= wr_bytes;
        }
    }

    retExit = cgi_chunk(req, &Pipe, PIPE_BUFSIZE);

    DisconnectNamedPipe(Pipe.parentPipe);
    CloseHandle(Pipe.parentPipe);
    CloseHandle(Pipe.hEvent);
    return retExit;
errExit:
    req->connKeepAlive = 0;
    return retExit;
}
//======================================================================
int cgi_chunk(Connect* req, PIPENAMED* Pipe, int maxRd)
{
    int n, ReadFromScript = 0;
    char buf[512], * ptr_buf;
    bool broken_pipe = false;
    int chunked = ((req->httpProt == HTTP11) && req->connKeepAlive) ? 1 : 0;
    ClChunked chunk(req->clientSocket, chunked);
    //------------ read from script ------------
    req->resp.respStatus = RS200;
    n = ReadFromPipe(Pipe, buf, sizeof(buf) - 1, &ReadFromScript, maxRd, conf->TimeOutCGI);
    if (n < 0)
    {
        print_err(req, "%d<%s:%d> Error script_to_buf()=%d\n", __func__, __LINE__, ReadFromScript);
        if (n == -RS408)
            return -RS500;
        return -1;
    }

    if (n == 0)
        broken_pipe = true;
    Array <string> hdrs(16);
    buf[ReadFromScript] = 0;
    //-------------------create headers of response---------------------
    ptr_buf = buf;
    for (; ; )
    {
        int len;
        char* p2, s[256];

        len = 0;
        while (1)
        {
            if ((*ptr_buf == '\0') || (len >= 256))
            {
                print_err(req, "<%s:%d>*** Error ***\n", __func__, __LINE__);
                return -1;
            }

            ReadFromScript--;
            if ((*ptr_buf) == '\n')
            {
                ptr_buf++;
                break;
            }
            if (*(ptr_buf) != '\r')
                s[len++] = *ptr_buf;
            ptr_buf++;
        }
        s[len] = '\0';
        if (len == 0)
            break;
        //print_err(req, "<%s:%d> %s\n", __func__, __LINE__, s);
        if (!strlcmp_case(s, "Status", 6))
        {
            if ((p2 = (char*)memchr(s, ':', len)))
            {
                req->resp.respStatus = strtol(++p2, NULL, 10);
                if (req->resp.respStatus == 0)
                    return -1;
                //  if (req->resp.respStatus == RS204)
                {
                    send_message(req, NULL, NULL);
                    return 0;
                }
            }
            continue;
        }

        if (!strlcmp_case(s, "Date", 4) || \
            !strlcmp_case(s, "Server", 6) || \
            !strlcmp_case(s, "Accept-Ranges", 13) || \
            !strlcmp_case(s, "Content-Length", 14) || \
            !strlcmp_case(s, "Connection", 10))
        {
            continue;
        }

        if (hdrs(s))
        {
            print_err(req, "<%s:%d> Error create_header()\n", __func__, __LINE__);
            return -1;
        }
    }
    //------------------------------------------------------------------
    req->resp.numPart = 0;
    req->resp.respContentType[0] = 0;
    req->resp.respContentLength = -1;

    if (chunked)
    {
        if (hdrs("Transfer-Encoding: chunked"))
        {
            return -1;
        }
    }

    if (send_response_headers(req, &hdrs))
    {
        return -1;
    }

    //------------------ send entity to client -------------------------
    if (ReadFromScript > 0)
    {
        int n = chunk.add_arr(ptr_buf, ReadFromScript);
        if (n < 0)
        {
            print_err(req, "<%s:%d> Error chunk.add_arr(): %d\n", __func__, __LINE__, n);
            return -1;
        }
    }

    if (!broken_pipe)
    {
        ReadFromScript = chunk.cgi_to_client(Pipe, maxRd);
        if (ReadFromScript < 0)
        {
            print_err(req, "<%s:%d> Error chunk.cgi_to_client()=%d\n", __func__, __LINE__, ReadFromScript);
            return -1;
        }
    }

    ReadFromScript = chunk.end();
    req->resp.send_bytes = chunk.all();
    if (ReadFromScript < 0)
    {
        print_err(req, "<%s:%d> Error chunk.end(): %d\n", __func__, __LINE__, ReadFromScript);
        return -1;
    }

    return 0;
}
