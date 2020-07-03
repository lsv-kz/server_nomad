#include "chunk.h"

#define FCGI_RESPONDER  1
//#define FCGI_AUTHORIZER 2
//#define FCGI_FILTER     3

//#define FCGI_KEEP_CONN  1

#define FCGI_VERSION_1           1
#define FCGI_BEGIN_REQUEST       1
#define FCGI_ABORT_REQUEST       2
#define FCGI_END_REQUEST         3
#define FCGI_PARAMS              4
#define FCGI_STDIN               5
#define FCGI_STDOUT              6
#define FCGI_STDERR              7
#define FCGI_DATA                8
#define FCGI_GET_VALUES          9
#define FCGI_GET_VALUES_RESULT  10
#define FCGI_UNKNOWN_TYPE       11
#define FCGI_MAXTYPE (FCGI_UNKNOWN_TYPE)

typedef struct {
	unsigned char type;
	int len;
	int paddingLen;
} fcgi_header;

const int requestId = 1;
//======================================================================
const int FCGI_SIZE_PAR_BUF = 4096 - 16;
const int FCGI_SIZE_HEADER = 8;

class FCGI_params
{
	char buf[FCGI_SIZE_HEADER + FCGI_SIZE_PAR_BUF + 8];
	int i = FCGI_SIZE_HEADER;
	SOCKET sock;
	
	int send_par(int end)
	{
		size_t len = i - FCGI_SIZE_HEADER;
		unsigned char padding = 8 - (len % 8);
		padding = (padding == 8) ? 0 : padding;
		
		char *p = buf;
		*p++ = FCGI_VERSION_1;
		*p++ = FCGI_PARAMS;
		*p++ = (unsigned char) ((1 >> 8) & 0xff);
		*p++ = (unsigned char) ((1) & 0xff);
	
		*p++ = (unsigned char) ((len >> 8) & 0xff);
		*p++ = (unsigned char) ((len) & 0xff);
	
		*p++ = (unsigned char)padding;
		*p = 0;
		
		memset(buf + i, 0, padding);
		i += padding;
		
		if ((end) && ((i + 8) <= (FCGI_SIZE_HEADER + FCGI_SIZE_PAR_BUF)))
		{
			char s[8] = {1, 4, 0, 1, 0, 0, 0, 0};
			memcpy(buf + i, s, 8);
			i += 8;
			end = 0;
		}

		int n = write_timeout(sock, buf, i, conf->TimeOutCGI);
		if (n == -1)
			return -1;

		i = FCGI_SIZE_HEADER;
		if (end)
			n = send_par(0);
		return n;
	}
	
public:
	FCGI_params(SOCKET s) { sock = s; }
	template <typename Arg>
	int add(const char *name, Arg val)
	{
		int ret = 0;
		if (!name)
		{
			ret = send_par(1);
			return ret;
		}
		ostringstream ss;
		ss << val;
		size_t len_name = strlen(name), len_val = ss.str().size(), len;
		
		len = len_name + len_val;
		len += len_name > 127 ? 4 : 1;
		len += len_val > 127 ? 4 : 1;
		
		if ((i + len) > (FCGI_SIZE_HEADER + FCGI_SIZE_PAR_BUF))
		{
			ret = send_par(0);
			if (ret < 0)
				return ret;
			if (len > FCGI_SIZE_PAR_BUF)
				return -1;
		}
		
		if (len_name < 0x80)
			*(buf + (i++)) = (unsigned char)len_name;
		else
		{
			*(buf + (i++)) = (unsigned char)((len_name >> 24) | 0x80);
			*(buf + (i++)) = (unsigned char)(len_name >> 16);
			*(buf + (i++)) = (unsigned char)(len_name >> 8);
			*(buf + (i++)) = (unsigned char)len_name;
		}
	
		if (len_val < 0x80)
			*(buf + (i++)) = (unsigned char)len_val;
		else
		{
			*(buf + (i++)) = (unsigned char)((len_val >> 24) | 0x80);
			*(buf + (i++)) = (unsigned char)(len_val >> 16);
			*(buf + (i++)) = (unsigned char)(len_val >> 8);
			*(buf + (i++)) = (unsigned char)len_val;
		}
		
		memcpy((buf + i), name, len_name);
		i += len_name;
		memcpy((buf + i), ss.str().c_str(), len_val);
		i += len_val;
		return ret;
	}
};
//======================================================================
SOCKET create_fcgi_socket(const wchar_t *host)
{
	char addr[256];
	char port[16] = "";
	std::string sHost;
	
	if (!host)
		return INVALID_SOCKET;
	
	if (utf16_to_mbs(sHost, host))
	{
		size_t sz = sHost.find(':');
		if (sz == std::string::npos)
		{
			print_err("<%s:%d> \n", __func__, __LINE__);
			return INVALID_SOCKET;
		}
		
		sHost.copy(addr, sz);
		addr[sz] = 0;
		
		size_t len = sHost.copy(port, sHost.size() - sz + 1, sz + 1);
		port[len] = 0;
	}
	else
	{
		print_err("<%s:%d> Error utf16_to_mbs()\n", __func__, __LINE__);
		return INVALID_SOCKET;
	}
//----------------------------------------------------------------------
	SOCKET sockfd;
	SOCKADDR_IN sock_addr;
	
	memset(&sock_addr, 0, sizeof(sock_addr));
	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sockfd == INVALID_SOCKET)
	{
		ErrorStrSock( __func__, __LINE__, "Error socket()");
		return INVALID_SOCKET;
	}

	sock_addr.sin_port = htons(atoi(port));
	sock_addr.sin_family = AF_INET;
	
	if (in4_aton(addr, &(sock_addr.sin_addr)) != 4)
	{
		print_err("<%s:%d> Error in4_aton()=%d\n", __func__, __LINE__);
		closesocket(sockfd);
		return INVALID_SOCKET;
	}

	if (connect(sockfd, (struct sockaddr *)(&sock_addr), sizeof(sock_addr)) == SOCKET_ERROR)
	{
		ErrorStrSock(__func__, __LINE__, "Error connect()");
		closesocket(sockfd);
		return INVALID_SOCKET;
	}
	
	return sockfd;
}
//======================================================================
int fcgi_to_stderr(SOCKET fcgi_sock, int cont_len, int timeout)
{
	int wr_bytes = 0;
	int rd;
	char buf[512];

	for( ; cont_len > 0; )
	{
		rd = read_timeout(fcgi_sock, buf, cont_len > (int)sizeof(buf) ? (int)sizeof(buf) : cont_len, timeout);
		if(rd == -1)
		{
			if (errno == EINTR)
				continue;
			return -1;
		}
		else if(rd == 0)
			break;
		
		cont_len -= rd;

		buf[rd] = 0;
		print_err("<%s:%d> %s\n", __func__, __LINE__, buf);
		wr_bytes += rd;
	}

	return wr_bytes;
} 
//======================================================================
void fcgi_set_header(char *header, int type, int id, size_t len, int padding_len)
{
	char *p = header;
	*p++ = FCGI_VERSION_1;                      // Protocol Version
	*p++ = type;                                // PDU Type
	*p++ = (unsigned char) ((id >> 8) & 0xff);  // Request Id
	*p++ = (unsigned char) ((id) & 0xff);       // Request Id
	
	*p++ = (unsigned char) ((len >> 8) & 0xff); // Content Length
	*p++ = (unsigned char) ((len) & 0xff);      // Content Length
	
	*p++ = (unsigned char)padding_len;                         // Padding Length
	*p = 0;                                   // Reserved
}
//======================================================================
int tail_to_fcgi(SOCKET fcgi_sock, char *tail, int lenTail)
{
	int rd, wr, all_wr = 0;
	const int size_buf = 4096;
	char buf[8 + size_buf], *p = tail;
	
	while(lenTail > 0)
	{
		if (lenTail > size_buf)
			rd = size_buf;
		else
			rd = lenTail;
		memcpy(buf + 8, p, rd);
		
		fcgi_set_header(buf, FCGI_STDIN, requestId, rd, 0);
		
		wr = write_timeout(fcgi_sock, buf, rd + 8, conf->TimeOutCGI);
		if (wr == -1)
		{
			return -1;
		}
		lenTail -= rd;
		all_wr += rd;
		p += rd;
	}
	return all_wr;
}
//======================================================================
int client_to_fcgi(SOCKET client_sock, SOCKET fcgi_sock, long long contentLength)
{
	int rd, wr, n;
	const int size_buf = 4096;
	char buf[8 + size_buf];
	
	while(contentLength > 0)
	{
		if (contentLength > size_buf)
			n = size_buf;
		else
			n = (int)contentLength;
		rd = read_timeout(client_sock, buf + 8, n, conf->TimeOut);
		if (rd == -1)
		{
			return -1;
		}
		
		fcgi_set_header(buf, FCGI_STDIN, requestId, rd, 0);
		
		wr = write_timeout(fcgi_sock, buf, rd + 8, conf->TimeOutCGI);
		if (wr == -1)
		{
			return -1;
		}
		contentLength -= rd;
	}
	return 0;
}
//======================================================================
int fcgi_get_header(SOCKET fcgi_sock, fcgi_header *header)
{
	int n;
	char buf[8];
	
	n = read_timeout(fcgi_sock, buf, 8, conf->TimeOutCGI);
	if (n <= 0)
	{
		print_err("<%s:%d> read_timeout()=%d\n", __func__, __LINE__, n);
		return n;
	}

	header->type = (unsigned char)buf[1];
	header->paddingLen = (unsigned char)buf[6];
	header->len = ((unsigned char)buf[4]<<8) | (unsigned char)buf[5];

	return n;
}
//======================================================================
int fcgi_chunk(request *req, SOCKET fcgi_sock, fcgi_header *header)
{
	int ret;
	int chunked = ((req->httpProt == HTTP11) && req->connKeepAlive) ? 1 : 0;
	ClChunked chunk(req->clientSocket, chunked);
//print_err("<%s:%d> -------------\n", __func__, __LINE__);
	req->resp.numPart = 0;
	req->resp.respContentType[0] = 0;
	req->resp.respContentLength = -1;

	if (chunked)
	{
		if (!create_header(req, "Transfer-Encoding: chunked", NULL))
		{
			print_err("%d<%s:%d> Error create_header()\n", req->numChld, __func__, __LINE__);
			return -RS500;
		}
	}

	if (send_header_response(req) < 0)
	{
		print_err("%d<%s:%d> Error send_header_response()\n", req->numChld, __func__, __LINE__);
		return -1;
	}
	//------------------- send entity after headers --------------------
	if (header->len > 0)
	{
		ret = chunk.fcgi_to_client(fcgi_sock, header->len);
		if (ret < 0)
		{
			print_err("%d<%s:%d> Error chunk_buf.fcgi_to_client()=%d\n", req->numChld, __func__, __LINE__, ret);
			return -1;
		}
	}

	if (header->paddingLen > 0)
	{
		char buf[256];
		ret = read_timeout(fcgi_sock, buf, header->paddingLen, conf->TimeOutCGI);
		if (ret <= 0)
		{
			print_err("%d<%s:%d> read_timeout()\n", req->numChld, __func__, __LINE__);
			return -1;
		}
	}
	//------------------- send entity other parts ----------------------
	while(1)
	{
		if (fcgi_get_header(fcgi_sock, header) <= 0)
			return -RS502;

		if (header->type == FCGI_END_REQUEST)
		{
			char buf[256];
			ret = read_timeout(fcgi_sock, buf, header->len, conf->TimeOutCGI);
			if (ret <= 0)
			{
				print_err("%d<%s:%d> read_timeout()=%d\n", req->numChld, __func__, __LINE__, ret);
				return -1;
			}
			
			break;
		}
		else if (header->type == FCGI_STDERR)
		{
			ret = fcgi_to_stderr(fcgi_sock, header->len, conf->TimeOutCGI);
			if (ret <= 0)
			{
				print_err("%d<%s:%d> fd_to_stream()\n", req->numChld, __func__, __LINE__);
				return -RS502;
			}
		}
		else if (header->type == FCGI_STDOUT)
		{
			ret = chunk.fcgi_to_client(fcgi_sock, header->len);
			if (ret < 0)
			{
				print_err("%d<%s:%d> Error chunk_buf.fcgi_to_client()=%d\n", req->numChld, __func__, __LINE__, ret);
				return -1;
			}
		}
		else
		{
			print_err("%d<%s:%d> Error fcgi: type=%hhu\n", req->numChld, __func__, __LINE__, header->type);
			return -1;
		}
		
		if (header->paddingLen > 0)
		{
			char buf[256];
			ret = read_timeout(fcgi_sock, buf, header->paddingLen, conf->TimeOutCGI);
			if (ret <= 0)
			{
				print_err("%d<%s:%d> read_timeout()=%d\n", req->numChld, __func__, __LINE__, ret);
				return -1;
			}
		}
	}
	//------------------------------------------------------------------
	ret = chunk.end();
	req->resp.send_bytes = chunk.all();
	if (ret < 0)
		print_err("%d<%s:%d> Error chunk.end()\n", req->numChld, __func__, __LINE__);

	return 0;
}
//======================================================================
int fcgi_read_headers(request *req, SOCKET fcgi_sock)
{
	int n, ret;
	fcgi_header header;
//print_err("<%s:%d> -------------\n", __func__, __LINE__);
	req->resp.respStatus = RS200;
	
	while (1)
	{
		if (fcgi_get_header(fcgi_sock, &header) <= 0)
			return -RS502;

		if (header.type == FCGI_STDOUT)
			break;
		else if (header.type == FCGI_STDERR)
		{
			n = fcgi_to_stderr(fcgi_sock, header.len, conf->TimeOutCGI);
			if (n <= 0)
			{
				print_err("%d<%s:%d> fcgi_to_stderr()=%d\n", req->numChld, __func__, __LINE__, n);
				return -RS502;
			}
			
			if (header.paddingLen > 0)
			{
				char buf[256];
				n = read_timeout(fcgi_sock, buf, header.paddingLen, conf->TimeOutCGI);
				if (n <= 0)
					return -RS502;
			}
		}
		else
		{
			print_err("%d<%s:%d> Error: %hhu\n", req->numChld, __func__, __LINE__, header.type);
			return -RS502;
		}
	}
	//-------------------------- read headers --------------------------
	for( ; header.len > 0; )
	{
		char *p2;
		char line[1024];
		ret = read_line_sock(fcgi_sock, line, sizeof(line)-1, conf->TimeOutCGI);
		if(ret <= 0)
		{
			return -RS500;
		}
		
		header.len -= ret;

		size_t i = strcspn(line, "\r\n");
		if(i == 0)
		{
			break;
		}

		line[i] = 0;
//print_err("<%d> %s\n", __LINE__, line);		
		if((p2 = strchr(line, ':')))
		{
			if(!strlcmp_case(line, "Status", 6))
			{
				req->resp.respStatus = strtol(++p2, NULL, 10);
			//	sscanf(++p2, "%d", &req->resp.respStatus); // ?
		//		if(req->resp.respStatus == RS204)
				{
					send_message(req, NULL);
					return 0;
				}
				continue;
			}
			else if(!strlcmp_case(line, "Date", 4) || \
				!strlcmp_case(line, "Server", 6) || \
				!strlcmp_case(line, "Accept-Ranges", 13) || \
				!strlcmp_case(line, "Content-Length", 14) || \
				!strlcmp_case(line, "Connection", 10))
			{
				print_err("%d<%s:%d> %s\n", req->numChld, __func__, __LINE__, line);
				continue;
			}
			
			if(!create_header(req, line, NULL))
			{
				print_err("%d<%s:%d> Error create_header()\n", req->numChld, __func__, __LINE__);
				return -RS500;
			}
			else if(strlcmp_case(line, "Content-Type", 12))
			{
				print_err("%d<%s:%d> %s\n", req->numChld, __func__, __LINE__, line);
			}
		}
		else
		{
			print_err("%d<%s:%d> Error: %s\n", req->numChld, __func__, __LINE__, line);
			return -RS500;
		}
	}
	
	return fcgi_chunk(req, fcgi_sock, &header);
}
//======================================================================
int fcgi_send_param(request *req, SOCKET fcgi_sock)
{
	int n;
	char buf[4096];
	string str, stmp;
	//------------------------- param ----------------------------------
	fcgi_set_header(buf, FCGI_BEGIN_REQUEST, requestId, 8, 0);
	
	buf[8] = (unsigned char) ((FCGI_RESPONDER >>  8) & 0xff);
	buf[9] = (unsigned char) (FCGI_RESPONDER         & 0xff);
	buf[10]=0;//(unsigned char) ((0) ? FCGI_KEEP_CONN : 0);
	buf[11]=0;
	buf[12]=0;
	buf[13]=0;
	buf[14]=0;
	buf[15]=0;
	
	n = write_timeout(fcgi_sock, buf, 16, conf->TimeOutCGI);
	if (n == -1)
	{
		print_err("%d<%s:%d> Error write_timeout()\n", req->numChld, __func__, __LINE__);
		return -RS502;
	}
	
	FCGI_params par(fcgi_sock);
/*
	if (par.add("PATH", getenv("PATH")) < 0)
		goto err_param;
*/	
	if (par.add("SERVER_SOFTWARE", conf->ServerSoftware) < 0)
		goto err_param;
	
	if (par.add("GATEWAY_INTERFACE", "CGI/1.1") < 0)
		goto err_param;
	
	utf16_to_mbs(str, conf->wRootDir.c_str());
	if (par.add("DOCUMENT_ROOT", str.c_str()) < 0)
		goto err_param;
	
	if (par.add("REMOTE_ADDR", req->remoteAddr) < 0)
		goto err_param;
	
	if (par.add("REMOTE_PORT", req->remotePort) < 0)
		goto err_param;
	
	utf16_to_mbs(str, req->wDecodeUri.c_str());
	if (par.add("REQUEST_URI", str.c_str()) < 0)
		goto err_param;

	if (req->resp.scriptType == php_fpm)
	{
		utf16_to_mbs(str, req->wDecodeUri.c_str());
		if (par.add("SCRIPT_NAME", str.c_str()) < 0)
			goto err_param;
	
		wstring wPath = conf->wRootDir;
		wPath += req->wScriptName;
		utf16_to_mbs(str, wPath.c_str());
		if (par.add("SCRIPT_FILENAME", str.c_str()) < 0)
			goto err_param;
	}	

	if (par.add("REQUEST_METHOD", get_str_method(req->reqMethod)) < 0)
		goto err_param;
	
	if (par.add("SERVER_PROTOCOL", get_str_http_prot(req->httpProt)) < 0)
		goto err_param;

	if(req->req_hdrs.iHost >= 0)
	{
		if (par.add("HTTP_HOST", req->req_hdrs.Value[req->req_hdrs.iHost]) < 0)
			goto err_param;
	}
	
	if(req->req_hdrs.iReferer >= 0)
	{
		if (par.add("HTTP_REFERER", req->req_hdrs.Value[req->req_hdrs.iReferer]) < 0)
			goto err_param;
	}
	
	if(req->req_hdrs.iUserAgent >= 0)
	{
		if (par.add("HTTP_USER_AGENT", req->req_hdrs.Value[req->req_hdrs.iUserAgent]) < 0)
			goto err_param;
	}

	if(req->reqMethod == M_POST)
	{
		if(req->req_hdrs.iReqContentType >= 0)
		{
			if (par.add("CONTENT_TYPE", req->req_hdrs.Value[req->req_hdrs.iReqContentType]) < 0)
				goto err_param;
		}
		
		if(req->req_hdrs.iReqContentLength >= 0)
		{
			if (par.add("CONTENT_LENGTH", req->req_hdrs.Value[req->req_hdrs.iReqContentLength]) < 0)
				goto err_param;
		}
	}
	
	if (par.add("QUERY_STRING", req->sReqParam) < 0)
		goto err_param;
	
	if (par.add(NULL, 0) < 0)
		goto err_param;
	
	if(req->reqMethod == M_POST)
	{
		if (req->tail)
		{
			n = tail_to_fcgi(fcgi_sock, req->tail, req->lenTail);
			if(n < 0)
			{
				print_err("%d<%s:%d> Error tail to script: %d\n", req->numChld,
						__func__, __LINE__, n);
				return -RS502;
			}
			req->req_hdrs.reqContentLength -= n;
		}
		
		n = client_to_fcgi(req->clientSocket, fcgi_sock, req->req_hdrs.reqContentLength);
		if (n == -1)
		{
			print_err("%d<%s:%d> Error client_to_fcgi()\n", req->numChld, __func__, __LINE__);
			return -RS502;
		}
	}

	return fcgi_read_headers(req, fcgi_sock);

err_param:
	print_err("%d<%s:%d> Error send_param()\n", req->numChld, __func__, __LINE__);
	return -RS502;
}
//======================================================================
int fcgi(request *req)
{
	SOCKET  fcgi_sock;
	int ret = 0;

	if(req->reqMethod == M_POST)
	{
		if(req->req_hdrs.iReqContentLength < 0)
		{
	//		print_err("%d<%s:%d> 411 Length Required\n", req->numChld, __func__, __LINE__);
			return -RS411;
		}
		
		if(req->req_hdrs.reqContentLength > conf->ClientMaxBodySize)
		{
	//		print_err("%d<%s:%d> 413 Request entity too large:%lld\n", req->numChld, 
	//				  __func__, __LINE__, req->req_hdrs.reqContentLength);
			return -RS413;
		}

		if(req->req_hdrs.iReqContentType < 0)
		{
			print_err("%d<%s:%d> Content-Type \?\n", req->numChld, __func__, __LINE__);
			return -RS400;
		}
	}

	if (req->resp.scriptType == php_fpm)
	{
		fcgi_sock = create_fcgi_socket(conf->wPathPHP.c_str());
	}
	else
	{
		print_err("%d<%s:%d> req->scriptType ?\n", req->numChld, __func__, __LINE__);
		return -RS500;
	}
	
	if (fcgi_sock == INVALID_SOCKET)
	{
		print_err("%d<%s:%d> Error connect to fcgi\n", req->numChld, __func__, __LINE__);
		return -RS500;
	}

	ret = fcgi_send_param(req, fcgi_sock);
	shutdown(fcgi_sock, SD_BOTH);
	closesocket(fcgi_sock);

	return ret;
}
