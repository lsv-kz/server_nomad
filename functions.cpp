#include "main.h"

using namespace std;

//======================================================================
int PrintError(const char *f, int line, const char *s)
{
    LPVOID lpMsgBuf;
    DWORD err = GetLastError();

    FormatMessage
    (
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        err,
        MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), // 1. LANG_NEUTRAL  
        (LPTSTR) &lpMsgBuf,
		0,
		NULL
	);
	
	if (lpMsgBuf)
	{
		print_err("<%s:%d> %s: (%ld)%s", f, line, s, err, (char*)lpMsgBuf);
		LocalFree(lpMsgBuf);
	}
	return err;
}
//======================================================================
int ErrorStrSock(const char *f, int line, const char *s)
{
    LPVOID lpMsgBuf;
    int err = WSAGetLastError();

    FormatMessage
    (
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        err,
        MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), // 1. LANG_NEUTRAL  
        (LPTSTR) &lpMsgBuf,
		0,
		NULL
	);
	
	if (lpMsgBuf)
	{
		print_err("<%s:%d> %s: (%ld)%s", f, line, s, err, (char*)lpMsgBuf);
		LocalFree(lpMsgBuf);
	}
	return err;
}
//======================================================================
string get_time()
{
	__time32_t now = 0;
	struct tm t;
	char s[40];

	_time32(&now);
	_gmtime32_s(&t, &now);
	//	stringstream ss;
	//	ss << put_time(&t, "%a, %d %b %Y %H:%M:%S GMT");
	//	return ss.str();
	string ss;
	strftime(s, sizeof(s), "%a, %d %b %Y %H:%M:%S GMT", &t);
	ss = s;
	return ss;
}
//======================================================================
char *strstr_case(const char *str1, const char *str2)
{
	char c1, c2, *s1, *s2, *p1, *p2;
	
	s1 = (char*)str1;
	s2 = (char*)str2;
	
	if (!s1 || !s2) return NULL;
	if (*s2 == 0) return s1;

	for (; ; ++s1)
	{
		c1 = *s1;
		if (!c1) break;
		c2 = *s2;
		c1 += (c1 >= 'A') && (c1 <= 'Z') ? ('a' - 'A') : 0;
		c2 += (c2 >= 'A') && (c2 <= 'Z') ? ('a' - 'A') : 0;
		if (c1 == c2)
		{
			p1 = s1;
			p2 = s2;
			++s1;
			++p2;

			for (; ; ++s1, ++p2)
			{
				c2 = *p2;
				if (!c2) return p1;
				
				c1 = *s1;
				if (!c1) return NULL;

				c1 += (c1 >= 'A') && (c1 <= 'Z') ? ('a' - 'A') : 0;
				c2 += (c2 >= 'A') && (c2 <= 'Z') ? ('a' - 'A') : 0;
				if (c1 != c2)
					break;
			}
		}
	}

	return NULL;
}
//======================================================================
int strlcmp_case(const char *s1, const char *s2, int len)
{
	char c1, c2;
	
	if (!s1 && !s2) return 0;
	if (!s1) return -1;
	if (!s2) return 1;

	for (; len > 0; --len, ++s1, ++s2)
	{
		c1 = *s1;
		c2 = *s2;
		if (!c1 && !c2) return 0;
		if (!c1) return -1;
		if (!c2) return 1;
		
		c1 += (c1 >= 'A') && (c1 <= 'Z') ? ('a' - 'A') : 0;
		c2 += (c2 >= 'A') && (c2 <= 'Z') ? ('a' - 'A') : 0;
		
		if (c1 > c2) return 1;
		if (c1 < c2) return -1;
	}

	return 0;
}
//======================================================================
int strcmp_case(const char *s1, const char *s2)
{
	char c1, c2;
	
	if (!s1 && !s2) return 0;
	if (!s1) return -1;
	if (!s2) return 1;

	for (; ; ++s1, ++s2)
	{
		c1 = *s1;
		c2 = *s2;
		if (!c1 && !c2) return 0;
		if (!c1) return -1;
		if (!c2) return 1;
		
		c1 += (c1 >= 'A') && (c1 <= 'Z') ? ('a' - 'A') : 0;
		c2 += (c2 >= 'A') && (c2 <= 'Z') ? ('a' - 'A') : 0;
		
		if (c1 > c2) return 1;
		if (c1 < c2) return -1;
	}

	return 0;
}
//======================================================================
int get_int_method(char *s)
{
	if (!memcmp(s, (char*)"GET", 3))
		return M_GET;
	else if (!memcmp(s, (char*)"POST", 4))
		return M_POST;
	else if (!memcmp(s, (char*)"HEAD", 4))
        	return M_HEAD;
	else if (!memcmp(s, (char*)"OPTIONS", 7))
		return M_OPTIONS;
	else if (!memcmp(s, (char*)"CONNECT", 7))
		return M_CONNECT;
	else
		return 0;
}
//======================================================================
char *get_str_method(int i)
{
	if (i == M_GET)
		return (char*)"GET";
	else if (i == M_POST)
		return (char*)"POST";
	else if (i == M_HEAD)
		return (char*)"HEAD";
	else if (i == M_OPTIONS)
		return (char*)"OPTIONS";
	else if (i == M_CONNECT)
		return (char*)"CONNECT";
	return (char*)"";
}
//======================================================================
int get_int_http_prot(char *s)
{
	if (!memcmp(s, (char*)"HTTP/1.1", 8))
		return HTTP11;
	else if (!memcmp(s, (char*)"HTTP/1.0", 8))
		return HTTP10;
	else if (!memcmp(s, (char*)"HTTP/0.9", 8))
        	return HTTP09;
	else if (!memcmp(s, (char*)"HTTP/2", 6))
		return HTTP2;
	else
		return 0;
}
//======================================================================
char *get_str_http_prot(int i)
{

	if (i == HTTP11)
		return (char*)"HTTP/1.1";
	else if (i == HTTP10)
		return (char*)"HTTP/1.0";
	else if (i == HTTP09)
		return (char*)"HTTP/0.9";
	else if (i == HTTP2)
		return (char*)"HTTP/2";
	return (char*)"";
}
//======================================================================
char *strstr_lowercase(const char * s1, const char *s2)
{
	int i, len = (int)strlen(s2);
	char *p = (char*)s1;
	for( i = 0; *p; ++p)
	{
		if(tolower(*p) == tolower(s2[0]))
		{
			for( i = 1; ; i++)
			{
				if(i == len)
					return p;
				if(tolower(p[i]) != tolower(s2[i]))
					break;
			}
		}
	}
	return NULL;
}
//======================================================================
const char *content_type(const wchar_t *path)
{
	string s;
	if (utf16_to_utf8(s, path))
	{
		print_err("<%s:%d> Error utf16_to_mbs()\n", __func__, __LINE__);
		return "";
	}
	
	const char *p = strrchr(s.c_str(), '.');
	if(!p)
	{
		return "";
	}
	//       video
	if(!strlcmp_case(p, ".ogv", 4)) return "video/ogg";
	else if(!strlcmp_case(p, ".mp4", 4)) return "video/mp4";
	else if(!strlcmp_case(p, ".avi", 4)) return "video/x-msvideo";
	else if(!strlcmp_case(p, ".mov", 4)) return "video/quicktime";
	else if(!strlcmp_case(p, ".mkv", 4)) return "video/x-matroska";
	else if(!strlcmp_case(p, ".flv", 4)) return "video/x-flv";
	else if(!strlcmp_case(p, ".mpeg", 5) || !strlcmp_case(p, ".mpg", 4)) return "video/mpeg";
	else if(!strlcmp_case(p, ".asf", 4)) return "video/x-ms-asf";
	else if(!strlcmp_case(p, ".wmv", 4)) return "video/x-ms-wmv";
	else if(!strlcmp_case(p, ".swf", 4)) return "application/x-shockwave-flash";
	else if(!strlcmp_case(p, ".3gp", 4)) return "video/video/3gpp";

	//       sound
	else if(!strlcmp_case(p, ".mp3", 4)) return "audio/mpeg";
	else if(!strlcmp_case(p, ".wav", 4)) return "audio/x-wav";
	else if(!strlcmp_case(p, ".ogg", 4)) return "audio/ogg";
	else if(!strlcmp_case(p, ".pls", 4)) return "audio/x-scpls";
	else if(!strlcmp_case(p, ".aac", 4)) return "audio/aac";
	else if(!strlcmp_case(p, ".aif", 4)) return "audio/x-aiff";
	else if(!strlcmp_case(p, ".ac3", 4)) return "audio/ac3";
	else if(!strlcmp_case(p, ".voc", 4)) return "audio/x-voc";
	else if(!strlcmp_case(p, ".flac", 5)) return "audio/flac";
	else if(!strlcmp_case(p, ".amr", 4)) return "audio/amr";
	else if(!strlcmp_case(p, ".au", 3)) return "audio/basic";
	
	//       image
	else if(!strlcmp_case(p, ".gif", 4)) return "image/gif";
	else if(!strlcmp_case(p, ".svg", 4) || !strlcmp_case(p, ".svgz", 5)) return "image/svg+xml";
	else if(!strlcmp_case(p, ".png", 4)) return "image/png";
	else if(!strlcmp_case(p, ".ico", 4)) return "image/vnd.microsoft.icon";
	else if(!strlcmp_case(p, ".jpeg", 5) || !strlcmp_case(p, ".jpg", 4)) return "image/jpeg";
	else if(!strlcmp_case(p, ".djvu", 5) || !strlcmp_case(p, ".djv", 4)) return "image/vnd.djvu";
	else if(!strlcmp_case(p, ".tiff", 5)) return "image/tiff";
	//       text
	else if(!strlcmp_case(p, ".txt", 4)) return "text/plain; charset=utf-8"; // return istextfile(s);
	else if(!strlcmp_case(p, ".html", 5) || !strlcmp_case(p, ".htm", 4) || !strlcmp_case(p, ".shtml", 6)) return "text/html; charset=utf-8"; // cp1251
	else if(!strlcmp_case(p, ".css", 4)) return "text/css";

	//       application
	else if(!strlcmp_case(p, ".pdf", 4)) return "application/pdf";
	else if(!strlcmp_case(p, ".gz", 3)) return "application/gzip";

	return "";
}
//======================================================================
int clean_path(char *path)
{
	int slash = 0, comma = 0, cnt = 0;
	char *p = path, ch;

	while((ch = *path))
	{
		if(ch == '/')
		{
			if (!slash)
			{
				*p++ = *path++;
				slash = 1;
				++cnt;
			}
			else path++;
			comma = 0;
		}
		else if (ch == '.')
		{
			if (comma && slash)
			{
				*p = 0;
				return 0;
			}

			comma = 1;
			
			if (slash) path++;
			else
			{
				*p++ = *path++;
				++cnt;
			}
		}
		else
		{
			if (comma && slash)
			{
				*p = 0;
				return 0;
			}
			*p++ = *path++;
			slash = comma = 0;
			++cnt;
		}
	}
	*p = 0;
	return cnt;
}
//======================================================================
int parse_startline_request(request *req, char *s, int len)
{
	char *p, tmp[16];
	//----------------------------- method -----------------------------
	p = tmp;
	int i = 0, n = 0;
	while (i < len)
	{
		char ch = s[i++];
		if ((ch != '\x20') && (ch != '\r') && (ch != '\n'))
			p[n++] = ch;
		else
			break;
	}
	p[n] = 0;
	req->reqMethod = get_int_method(tmp);
	if (!req->reqMethod) return -RS400;
	//------------------------------- uri ------------------------------
	char ch = s[i];
	if ((ch == '\x20') || (ch == '\r') || (ch == '\n'))
	{
		return -RS400;
	}

	req->uri = s + i;
	n = 0;
	while (i < len)
	{
		char ch = s[i];
		if ((ch != '\x20') && (ch != '\r') && (ch != '\n'))
			n++;
		else
			break;
		++i;
	}
	
	if (s[i] == '\r')
	{
		// HTTP/0.9
		req->httpProt = HTTP09;
		s[i] = 0;
		return 0;
	}
	
	if (s[i] == '\x20')
		s[i++] = 0;
	else
		return -RS400;
	//------------------------------ version ---------------------------
	ch = s[i];
	if ((ch == '\x20') || (ch == '\r') || (ch == '\n'))
		return -RS400;

	p = tmp;
	n = 0;
	while (i < len)
	{
		char ch = s[i++];
		if ((ch != '\x20') && (ch != '\r') && (ch != '\n'))
			p[n++] = ch;
		else
			break;
	}
	p[n] = 0;
	
	if(!(req->httpProt = get_int_http_prot(tmp)))
	{
		print_err("(%d/%d)<%s():%d> Error version protocol\n", req->numReq, 
			req->numConn, __FUNCTION__, __LINE__);
		req->httpProt = HTTP11;
		return -RS400;
	}

	return 0;
}
//======================================================================
int parse_headers(request *req, char *s, int len)
{
	int n;
	char *pName = s, *pVal, *p;
	
	p = (char*)memchr(pName, '\r', len);
	if (!p) return -1;
	*p = 0;
	
	if(!(p = (char*)memchr(pName, ':', len)))
	{
		print_err("%d<%s:%d> Error: ':' not found\n", req->numChld, __func__, __LINE__);
		return -RS400;
	}
	*p = 0;

	n = (int)strspn(p + 1, "\x20");
	pVal = p + 1 + n;
	
	if(!strlcmp_case(pName, "connection", 32))
	{
		req->req_hdrs.iConnection = req->req_hdrs.countReqHeaders;
		if(strstr_case(pVal, "keep-alive"))
			req->connKeepAlive = 1;
		else
			req->connKeepAlive = 0;
	}
	else if(!strlcmp_case(pName, "host", 32))
	{
		req->req_hdrs.iHost = req->req_hdrs.countReqHeaders;
	}
	else if(!strlcmp_case(pName, "range", 32))
	{
		sscanf_s(pVal, "bytes=%63s", req->sRange, (unsigned)_countof(req->sRange));
		req->req_hdrs.iRange = req->req_hdrs.countReqHeaders;
	}
	else if(!strlcmp_case(pName, "If-Range", 32))
	{
		req->req_hdrs.iIf_Range = req->req_hdrs.countReqHeaders;
	}
	else if(!strlcmp_case(pName, "referer", 32))
	{
		req->req_hdrs.iReferer = req->req_hdrs.countReqHeaders;
	}
	else if(!strlcmp_case(pName, "user-agent", 32))
	{
		req->req_hdrs.iUserAgent = req->req_hdrs.countReqHeaders;
	}
	else if(!strlcmp_case(pName, "upgrade", 32))
	{
		req->req_hdrs.iUpgrade = req->req_hdrs.countReqHeaders;
	}
	else if(!strlcmp_case(pName, "content-length", 32))
	{
		req->req_hdrs.reqContentLength = atoll(pVal);
		req->req_hdrs.iReqContentLength = req->req_hdrs.countReqHeaders;
	}
	else if(!strlcmp_case(pName, "content-type", 32))
	{
		req->req_hdrs.iReqContentType = req->req_hdrs.countReqHeaders;
	}
	else if(!strlcmp_case(pName, "accept-encoding", 32))
	{
		req->req_hdrs.iAcceptEncoding = req->req_hdrs.countReqHeaders;
	}

	req->req_hdrs.Name[req->req_hdrs.countReqHeaders] = pName;
	req->req_hdrs.Value[req->req_hdrs.countReqHeaders] = pVal;
	++req->req_hdrs.countReqHeaders;
	req->req_hdrs.Name[req->req_hdrs.countReqHeaders] = NULL;
	req->req_hdrs.Value[req->req_hdrs.countReqHeaders] = NULL;

	return 0;
}
//======================================================================
#include <iomanip>
//======================================================================
string hex_dump(void *p, int n)
{
	int count, addr = 0, col;
	if (!p) return "";
	unsigned char *buf = (unsigned char*)p;
	char str[18];
	stringstream ss;
	string s, s_out;

	for(count = 0; count < n;)
	{
		ss << uppercase << fixed << right << setw(8) << setfill('0') << hex << addr << "  ";
		getline(ss, s);
		ss.clear();
		s_out += s;
		for(col = 0, addr = addr + 0x10; (count < n) && (col < 16); count++, col++)
		{
			ss << uppercase << fixed << right << setw(2) << setfill('0') << hex << (int)*(buf+count) << " "; 
			getline(ss, s);
			s_out += s;
			ss.clear();
			str[col] = (*(buf + count) >= 32 && *(buf + count) < 127) ? *(buf + count) : '.';
		}
		str[col] = 0;
     	ss << "  " << setfill(' ') << setw(((16 - (col))) * 3) << ""  << str << ""; 
     	getline(ss, s);
     	s_out += (s + "\n");
		ss.clear();
	}
//	s_out += "\n";
	return s_out;
}
//======================================================================
void path_correct(wstring& path)
{
	size_t len = path.size(), i = 0;
	while (i < len)
	{
		if (path[i] == '\\')
			path[i] = '/';
		++i;
	}
}

