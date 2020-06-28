#include "main.h"

static HANDLE hLog, hLogErr;
//======================================================================
void create_logfiles(const wchar_t *log_dir, const char * name, HANDLE *h, HANDLE *hErr)
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
	
	if(hLog == INVALID_HANDLE_VALUE)
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
	
	if(hLogErr == INVALID_HANDLE_VALUE)
	{
		DWORD err = GetLastError();
		printf("<%s:%d>  Error create_logfiles(): %lu\n", __func__, __LINE__, err);
		exit(1);
	}
	
	*hErr = hLogErr;
}
//======================================================================
void mprint_err(const char *format, ...)
{
	va_list ap;
	char buf[256 * 2];

	va_start(ap, format);
	vsnprintf(buf, 256 * 2, format, ap);
	va_end(ap);
	
	stringstream ss;
	ss << "[" << get_time() << "] - " << buf;

	DWORD wrr;
	BOOL res = WriteFile(hLogErr, ss.str().c_str(), (DWORD)ss.str().size(), &wrr, NULL);
	if (!res)
	{
		DWORD err = GetLastError();
		printf("<%s:%d>  Error WriteFile(): %lu\n", __func__, __LINE__, err);
		exit(1);
	}
}
//======================================================================
int  nChld;
static HANDLE hChildLog, hChildLogErr;
mutex mtxLog;
//======================================================================
HANDLE open_logfiles(HANDLE h, HANDLE hErr)
{
	hChildLog = h;
	hChildLogErr = hErr;
	HANDLE hLogErrDup;
	BOOL res = DuplicateHandle(
					GetCurrentProcess(), 
                    hChildLogErr, 
                    GetCurrentProcess(),
                    &hLogErrDup, 
                    0,
                    TRUE,
                    DUPLICATE_SAME_ACCESS);
	if (!res)
	{
		exit(1);
	}
	
	return hLogErrDup;
}
//======================================================================
void print_err(const char *format, ...)
{
	va_list ap;
	char buf[256 * 2];

	va_start(ap, format);
	vsnprintf(buf, 256 * 2, format, ap);
	va_end(ap);
	
	stringstream ss;
	ss << "[" << get_time() << "] - " << buf;
	
lock_guard<mutex> l(mtxLog);
	DWORD wrr;
	BOOL res = WriteFile(hChildLogErr, ss.str().c_str(), (DWORD)ss.str().size(), &wrr, NULL);
	if (!res)
	{
		exit(1);
	}
}
//======================================================================
void print_log(request *req)
{
	stringstream ss;
		
	ss << req->numChld << "/" << req->numConn << "/" << req->numReq << " - " << req->remoteAddr << ":" << req->remotePort
		<< " - [" << req->resp.sLogTime.c_str() << "] - ";
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
	BOOL res = WriteFile(hChildLog, ss.str().c_str(), (DWORD)ss.str().size(), &wrr, NULL);
	if (!res)
	{
		exit(1);
	}
}

