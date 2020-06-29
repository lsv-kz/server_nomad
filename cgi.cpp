#include "chunk.h"

int cgi_chunk(request *req, PIPENAMED *Pipe, int maxRd);
//======================================================================
class CreateEnv
{
private:
	size_t sizeBufEnv;
	size_t i;
	char *bufEnv;

public:
	template <typename Val>
	size_t add (const char *name, Val val)
	{
		ostringstream ss;
		if (*name)
			ss << name << '=' << val << '\0';
		else
			ss << '\0';

		size_t len = ss.str().size();
		if ((i + len) >= sizeBufEnv)
		{
			int sizeTmp = sizeBufEnv + len + 128;
			char *tmp = new(nothrow) char[sizeTmp];
			if (!tmp)
				return 0;
			sizeBufEnv = sizeTmp;
			memcpy(tmp, bufEnv, i);
			delete[] bufEnv;
			bufEnv = tmp;
		}
		memcpy(bufEnv + i, ss.str().c_str(), len);
		i += len;
		bufEnv[i] = '\0';
		return len;
	}
	//------------------------------------------------------------------
	char *get() { return bufEnv; }
	//------------------------------------------------------------------
	size_t size() { return i+1; }
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
	const wchar_t *p;

	if ((p = wcschr(name.c_str() + 1, '/')))
	{
		return p;
	}
	return name;
}
//======================================================================
int cgi(request *req)
{
	int retExit = -1;
	size_t len;
	long long wr_bytes;
	struct _stat st;
	BOOL bSuccess;
	string str, stmp;
	
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
	else if(req->resp.scriptType == php_cgi)
	{
		wPath += conf->wRootDir; 
		wPath += req->wScriptName;
	}

	if(_wstat(wPath.c_str(), &st) == -1)
	{
		utf16_to_mbs(str, wPath.c_str());
		print_err("%d<%s:%d> script (%s) not found: errno=%d\n", req->numChld, __func__,
								__LINE__, str.c_str(), errno);
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
			print_err("%d<%s:%d> Error getenv_s()\n", req->numChld, __func__, __LINE__);
			retExit = -RS500;
			goto errExit;
		}
	}
/*
	{
		const int size = 4096;
		char tmpBuf[size];
		if (ExpandEnvironmentStringsA("PATH=%PATH%", tmpBuf, size))
		{
			env.add("PATH", tmpBuf);
		}
		else
		{
			print_err("%d<%s:%d> Error getenv_s()\n", req->numChld, __func__, __LINE__);
			retExit = -RS500;
			goto errExit;
		}
	}
*/
	if (req->reqMethod == M_POST)
	{
		if (req->req_hdrs.iReqContentLength >= 0)
			env.add("CONTENT_LENGTH", req->req_hdrs.Value[req->req_hdrs.iReqContentLength]);
		else
		{
	//		print_err("%d<%s:%d> 411 Length Required\n", req->numChld, __func__, __LINE__);
			retExit = -RS411;
			goto errExit;
		}
		
		if(req->req_hdrs.reqContentLength > conf->ClientMaxBodySize)
		{
			req->connKeepAlive = 0;
			retExit = -RS413;
			goto errExit;
		}
		
		if (req->req_hdrs.iReqContentType >= 0)
			env.add("CONTENT_TYPE", req->req_hdrs.Value[req->req_hdrs.iReqContentType]);
		else
		{
			print_err("%d<%s:%d> Content-Type \?\n", req->numChld, __func__, __LINE__);
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

	env.add("SERVER_PORT", conf->servPort);

	env.add("SERVER_SOFTWARE", conf->ServerSoftware);
	env.add( "GATEWAY_INTERFACE", "CGI/1.1");
	env.add("REQUEST_METHOD", get_str_method(req->reqMethod));
	env.add("REMOTE_HOST", req->remoteAddr);
	env.add("SERVER_PROTOCOL", get_str_http_prot(req->httpProt));
	
	utf16_to_mbs(str, conf->wRootDir.c_str()); 
	env.add("DOCUMENT_ROOT", str.c_str());
	
	utf16_to_mbs(str, req->wDecodeUri.c_str());
	env.add("REQUEST_URI", str.c_str());

	utf16_to_mbs(str, wPath.c_str());
	env.add("SCRIPT_FILENAME", str.c_str());
	
	utf16_to_mbs(str, req->wDecodeUri.c_str());
	env.add("SCRIPT_NAME", str.c_str());
	
	env.add("REMOTE_ADDR", req->remoteAddr);
	env.add("REMOTE_PORT", req->remotePort);

	if (req->sReqParam == NULL)
		env.add("QUERY_STRING", "");
	else
		env.add("QUERY_STRING", req->sReqParam);
	
	env.add("", 0);
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
			string tmp;
			utf16_to_mbs(tmp, commandLine.c_str());
		}
		else if (wcsstr(req->wScriptName, L".py"))
		{
			commandLine = conf->wPyPath;
			commandLine += L' ';
			commandLine += wPath;
			string tmp;
			utf16_to_mbs(tmp, commandLine.c_str());
		}
		else
		{
			commandLine = wPath;
			string tmp;
			utf16_to_mbs(tmp, commandLine.c_str());
		}
	}
	else
	{
		print_err("%d<%s:%d> Error CommandLine CreateProcess()\n", req->numChld, __func__, __LINE__);
		retExit = -RS500;
		goto errExit;
	}
	//------------------------------------------------------------------
	Pipe.hEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
	if (Pipe.hEvent == NULL) 
	{
		print_err("<%s:%d> CreateEvent failed with %lu\n", __func__, __LINE__, GetLastError()); 
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
		print_err("<%s:%d> CreateNamedPipe failed, GLE=%lu\n", __func__, __LINE__, GetLastError()); 
		retExit = -RS500;
		
		CloseHandle(Pipe.hEvent);
		
		goto errExit;
	}
	
	if (!SetHandleInformation(Pipe.parentPipe, HANDLE_FLAG_INHERIT, 0))
	{
		print_err("<%s:%d> Error SetHandleInformation, GLE=%lu\n", __func__, __LINE__, GetLastError());
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
		print_err("<%s:%d> Error CreateFile, GLE=%lu\n", __func__, __LINE__, GetLastError());
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
	si.hStdError = hLogErrDup;
	si.dwFlags |= STARTF_USESTDHANDLES;

	bSuccess = CreateProcessW(NULL, (wchar_t*)commandLine.c_str(), NULL, NULL, TRUE, 0, env.get(), NULL, &si, &pi);
	CloseHandle(childPipe);
	if (!bSuccess)
	{
		utf16_to_mbs(str, commandLine.c_str());
		print_err("%d<%s:%d> Error CreateProcessW(%s)\n", req->numChld, __func__, __LINE__, str.c_str());
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
				print_err("%d<%s:%d> Error write_to_script()=%d\n", req->numChld, __func__, __LINE__, wr_bytes);
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
				print_err("%d<%s:%d> Error client_to_script()\n", req->numChld, __func__, __LINE__);
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
int cgi_chunk(request *req, PIPENAMED *Pipe, int maxRd)
{
	int n, ReadFromScript = 0;
	char buf[512], *ptr_buf;
	bool broken_pipe = false;
	int chunked = ((req->httpProt == HTTP11) && req->connKeepAlive) ? 1 : 0;
	ClChunked chunk(req->clientSocket, chunked);
	//------------ read from script ------------
	req->resp.respStatus = RS200;
	n = ReadFromPipe(Pipe, buf, sizeof(buf) - 1, &ReadFromScript, maxRd, conf->TimeOutCGI);
	if (n < 0)
	{
		print_err("%d<%s:%d> Error script_to_buf()=%d\n", req->numChld, __func__, __LINE__, ReadFromScript);
		if (n == -RS408)
			return -RS500;
		return -1;
	}
	
	if (n == 0)
		broken_pipe = true;

	buf[ReadFromScript] = 0;
	//-------------------create headers of response---------------------
	ptr_buf = buf;
	for (; ; )
	{
		int len;
		char *p2, s[256];

		len = 0;
		while (1)
		{
			if ((*ptr_buf == '\0') || (len >= 256))
			{
				print_err("<%s:%d>*** Error ***\n", __func__, __LINE__);
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
//print_err("<%s:%d> %s\n", __func__, __LINE__, s);
		if (!strlcmp_case(s, "Status", 6))
		{
			if ((p2 = (char*)memchr(s, ':', len)))
			{
				req->resp.respStatus = strtol(++p2, NULL, 10);
				if (req->resp.respStatus == 0)
					return -1;
			//	if (req->resp.respStatus == RS204)
				{
					send_message(req, NULL);
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

		if (!create_header(req, s, NULL))
		{
			print_err("%d<%s:%d> Error create_header()\n", req->numChld, __func__, __LINE__);
			return -1;
		}
	}
	//------------------------------------------------------------------
	req->resp.numPart = 0;
	req->resp.respContentType[0] = 0;
	req->resp.respContentLength = -1;

	if (chunked)
	{
		if (!create_header(req, "Transfer-Encoding: chunked", NULL))
		{
			return -1;
		}
	}

	if (send_header_response(req) < 0)
	{
		return -1;
	}

	//------------------ send entity to client -------------------------
	if (ReadFromScript > 0)
	{
		int n = chunk.add_arr(ptr_buf, ReadFromScript);
		if (n < 0)
		{
			print_err("%d<%s:%d> Error chunk.add_arr(): %d\n", req->numChld, __func__, __LINE__, n);
			return -1;
		}
	}

	if (!broken_pipe)
	{
		ReadFromScript = chunk.cgi_to_client(Pipe, maxRd);
		if (ReadFromScript < 0)
		{
			print_err("%d<%s:%d> Error chunk.cgi_to_client()=%d\n", req->numChld, __func__, __LINE__, ReadFromScript);
			return -1;
		}
	}

	ReadFromScript = chunk.end();
	req->resp.send_bytes = chunk.all();
	if (ReadFromScript < 0)
	{
		print_err("<%s:%d>   Error chunk.end(): %d\n", __func__, __LINE__, ReadFromScript);
		return -1;
	}

	return 0;
}