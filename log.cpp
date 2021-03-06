#include "main.h"
#include <sstream>

using namespace std;

static HANDLE hLog, hLogErr;
//======================================================================
void create_logfiles(const wchar_t* log_dir, HANDLE* h, HANDLE* hErr)
{
    wstringstream ss;
    ss << log_dir << L"/" << "access.log";

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    hLog = CreateFileW(
        ss.str().c_str(),
        GENERIC_WRITE,  // FILE_APPEND_DATA, // | GENERIC_READ,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        &saAttr,
        CREATE_ALWAYS, // OPEN_ALWAYS, // | TRUNCATE_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hLog == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        printf("<%s:%d>  Error create_logfiles(): %lu\n", __func__, __LINE__, err);
        wcerr << ss.str() << L"\n";
        exit(1);
    }

    *h = hLog;
    //------------------------------------------------------------------
    ss.str(L"");
    ss.clear();

    ss << log_dir << L"/" << "error.log";
    hLogErr = CreateFileW(
        ss.str().c_str(),
        GENERIC_WRITE,  // FILE_APPEND_DATA, // | GENERIC_READ,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        &saAttr,
        CREATE_ALWAYS, // OPEN_ALWAYS, // | TRUNCATE_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hLogErr == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        printf("<%s:%d>  Error create_logfiles(): %lu\n", __func__, __LINE__, err);
        exit(1);
    }

    *hErr = hLogErr;
}
//======================================================================
int  nChld;
static HANDLE hChildLog, hChildLogErr;
mutex mtxLog;
//======================================================================
void open_logfiles(HANDLE h, HANDLE hErr)
{
    hLog = h;
    hLogErr = hErr;
}
//======================================================================
void print_err(const char* format, ...)
{
    va_list ap;
    char buf[256 * 2];

    va_start(ap, format);
    vsnprintf(buf, 256 * 2, format, ap);
    va_end(ap);

    String ss(256);
    ss << "[" << get_time().str() << "] - " << buf;

    lock_guard<mutex> l(mtxLog);
    DWORD wrr;
    BOOL res = WriteFile(hLogErr, ss.str(), (DWORD)ss.len(), &wrr, NULL);
    if (!res)
    {
        exit(1);
    }
}
//======================================================================
void print_err(Connect* req, const char* format, ...)
{
    va_list ap;
    char buf[256 * 2];

    va_start(ap, format);
    vsnprintf(buf, 256 * 2, format, ap);
    va_end(ap);

    String ss(512);
    ss << "[" << get_time().str() << "]-[" << req->numChld << "/" << req->numConn << "/" << req->numReq << "] " << buf;

    lock_guard<mutex> l(mtxLog);
    DWORD wrr;
    BOOL res = WriteFile(hLogErr, ss.str(), (DWORD)ss.len(), &wrr, NULL);
    if (!res)
    {
        exit(1);
    }
}
//======================================================================
void print_log(Connect* req)
{
    String ss(512);

    ss << req->numChld << "/" << req->numConn << "/" << req->numReq << " - " << req->remoteAddr // << ":" << req->remotePort
        << " - [" << req->resp.sLogTime.str() << "] - ";
    if (req->reqMethod > 0)
        ss << "\"" << get_str_method(req->reqMethod) << " "
        << req->decodeUri << " " << get_str_http_prot(req->httpProt) << "\" "; // uri
    else
        ss << "\"-\" ";

    ss << req->resp.respStatus << " " << req->resp.send_bytes << " "
        << "\"" << ((req->req_hdrs.iReferer >= 0) ? req->req_hdrs.Value[req->req_hdrs.iReferer] : "-") << "\" "
        << "\"" << ((req->req_hdrs.iUserAgent >= 0) ? req->req_hdrs.Value[req->req_hdrs.iUserAgent] : "-")
        << "\"\n";
    lock_guard<mutex> l(mtxLog);
    DWORD wrr;
    BOOL res = WriteFile(hLog, ss.str(), (DWORD)ss.len(), &wrr, NULL);
    if (!res)
    {
        exit(1);
    }
}
//======================================================================
HANDLE GetHandleLogErr()
{
    return hLogErr;
}