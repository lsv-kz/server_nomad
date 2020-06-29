#ifndef SERVER_H_
#define SERVER_H_
#define _WIN32_WINNT  0x0501

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>

#include <mutex>
#include <thread>
#include <condition_variable>

#include <stdio.h>
#include <cstdlib>
#include <string.h>
#include <cassert>
#include <climits>
#include <wchar.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

#include <io.h>
#include <sys/types.h>
#include <share.h>

#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <time.h>
#include <Winsock2.h> // typedef u_int	SOCKET;
#include <winsock.h>
#include <ws2tcpip.h>
#include <direct.h>
#include <process.h>

using namespace std;


#define     MAX_NAME           256
#define     LEN_BUF_REQUEST    8192 // 8192 16284 32768
#define	    NUM_HEADERS         25
#define     BUF_TMP_SIZE      10000

enum {
	RS101 = 101,
	RS200 = 200,RS204 = 204,RS206 = 206,
	RS301 = 301, RS302,
	RS400 = 400,RS401,RS402,RS403,RS404,RS405,RS406,RS407,
	RS408,RS411 = 411,RS413 = 413,RS414,RS415,RS416,RS417,RS418,
	RS500 = 500,RS501,RS502,RS503,RS504,RS505
};

enum { cgi_ex = 1, php_cgi, php_fpm};

enum {
	M_GET = 1, M_HEAD, M_POST, M_OPTIONS, M_PUT,
	M_PATCH, M_DELETE, M_TRACE, M_CONNECT	
};

enum { HTTP09 = 1, HTTP10, HTTP11, HTTP2 };

enum {
	EXIT_THR = 1,
};

struct Range {
    long long start;
    long long end;
    long long part_len;
};

typedef struct 
{ 
	OVERLAPPED oOverlap; 
	HANDLE parentPipe; 
	HANDLE hEvent;
	DWORD dwState; 
	BOOL fPendingIO; 
} PIPENAMED;

struct Config
{
	std::string host = "0.0.0.0";
	std::string ServerSoftware = "nomad";
	std::string servPort = "8080";

	int SOCK_BUFSIZE = 16284;

	int NumChld = 1;
	int MaxThreads = 18;
	int MinThreads = 6;
	int MaxRequestsPerThr = 50;
	
	int ListenBacklog = 128;
	
	int SizeQueue = 480;

	char KeepAlive = 'y';
	int TimeoutKeepAlive = 5;
	int TimeOut = 30;
	int TimeOutCGI = 5;
	int TimeoutThreadCond = 5;
	
	std::wstring wLogDir = L"";
	std::wstring wRootDir = L"";
	std::wstring wCgiDir = L"";
	std::wstring wPerlPath = L"";
	std::wstring wPyPath = L"";

	std::string usePHP = "n";
	std::wstring wPathPHP = L"";
	
	long int ClientMaxBodySize = 1000000;

	char index_html = 'n';
	char index_php = 'n';
	char index_pl = 'n';

	char ShowMediaFiles = 'n';
};

extern const Config* const conf;

class request
{
public:
	static SOCKET serverSocket;

	unsigned int numReq, numConn;
	int       numChld, timeout;
	SOCKET    clientSocket;
	int       err;
	time_t    time_write;

	int       num_write;
	
	char      remoteAddr[NI_MAXHOST];
	char      remotePort[NI_MAXSERV];
	
	char      bufReq[LEN_BUF_REQUEST];
	
	char      *tail;
	int       lenTail;
	
	char      decodeUri[LEN_BUF_REQUEST];
	size_t    lenDecodeUri;
	
	std::wstring   wDecodeUri;

	char      *uri;
	size_t    uriLen;
	
	int       reqMethod;
	//---------------------- dynamic buffer ----------------------------
	const wchar_t    *wScriptName;
	//---------------------- end ---------------------------------------
	char      *sReqParam;
	char      sRange[64];
	int       httpProt;
	int       connKeepAlive;
	
	struct  {
		int       iConnection;
		int       iHost;
		int       iUserAgent;
		int       iReferer;
		int       iUpgrade;
		int       iReqContentType;
		int       iReqContentLength;
		int       iAcceptEncoding;
		int       iRange;
		int       iIf_Range;
		
		int       countReqHeaders;
		long long reqContentLength;
		
		char      *Name[NUM_HEADERS];
		char      *Value[NUM_HEADERS];
	} req_hdrs;
	/*--------------------------*/
	struct {
		int       respStatus;
		std::string    sLogTime;
		long long respContentLength;
		char      respContentType[128];
		long long fileSize;
		int       countRespHeaders;
		char      *respHeaders[NUM_HEADERS];
		
		int       scriptType;
		const char *scriptName;
		
		struct Range *rangeBytes;
		int       numPart;
		int       fd;
		long long offset;
		long long send_bytes;
	} resp;
	//----------------------------------------
	void init()
	{
		err = -1;
		bufReq[0] = '\0';
		decodeUri[0] = '\0';
		sRange[0] = '\0';
		sReqParam = NULL;
		//------------------------------------
		uri = NULL;
		//------------------------------------
		reqMethod = 0;
		httpProt = 0;
		connKeepAlive = 0;
		
		req_hdrs = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0, -1LL};
		req_hdrs.Name[0] = NULL;
		req_hdrs.Value[0] = NULL;
		
		resp.respStatus = 0;
		resp.fd = -1;
		resp.fileSize = 0;
		resp.offset = 0;
		resp.respContentLength = -1LL;
		resp.numPart = 0;
		resp.send_bytes = 0LL;
		resp.respContentType[0] = '\0';
		resp.scriptType = 0;
		resp.countRespHeaders = 0;
		resp.sLogTime[0] = '\0';
		resp.scriptName = NULL;
		resp.respHeaders[0] = NULL;
		resp.rangeBytes = NULL;
	}
	
	void free_resp_headers()
	{
		for (int i = 0; resp.respHeaders[i] && (i < resp.countRespHeaders); i++)
		{
			delete [] resp.respHeaders[i];
			resp.respHeaders[i] = NULL;
		}
	}
	
	void free_range()
	{
		if(resp.rangeBytes)
		{
			delete [] resp.rangeBytes;
			resp.rangeBytes = NULL;
		}
	}
};

class RequestManager
{
private:
	std::mutex mtx_qu, mtx_thr, mtx_close_req;
	std::condition_variable cond_push, cond_pop;
	std::condition_variable cond_close_req, cond_start_req, cond_exit_thr;
	int count_push, count_pop, num_wait_thr, len_qu;
	int count_thr, count_req, stop_manager, num_create_thr;
	int numChld;
	HANDLE hClose_out;

	unsigned long all_thr;
	request **quReq;
	
public:
	RequestManager(const RequestManager&) = delete;
	RequestManager(int, HANDLE pipe_out);
	~RequestManager();
	//-------------------------------
	int get_num_chld(void);
	int get_len_qu(void);
	int get_num_req(void);
	int get_num_thr(void);
	int get_all_req_thr(void);
	int start_thr(void);
	int exit_thr();
	void wait_exit_thr(int n);
	int start_req(void);
	void wait_close_req(int n);
	void timedwait_close_req(void);
	int push_req(request *req);
	request *pop_req();
	int end_req(int *nthr, int* nconn);
	int wait_new_req();
	int check_num_thr(int *nthr);
	void inc_all_thr() {++all_thr;}
	void close_manager();
	void close_connect(request*);
	void close_response(request*);
};

extern HANDLE hLogErrDup;
//======================================================================
int in4_aton(const char* host, struct in_addr* addr);
SOCKET create_server_socket(const Config *conf);

void get_request(RequestManager *ReqMan);
int response(RequestManager *ReqMan, request *req);
int options(request *req);
int index_dir(RequestManager *ReqMan, request *req, wstring& path);
int parse_range(request *req);

int decode(char *s_in, size_t len_in, char *s_out, int len);
//----------------------------------------------------------------------
int cgi(request *req);
int fcgi(request *req);
/*---------------------------- functions.c ---------------------------*/
int ErrorStrSock(const char *f, int line, const char *s);
int PrintError(const char *f, int line, const char *s);
string get_time();
char *strstr_case(const char * s1, const char *s2);
int strlcmp_case(const char *s1, const char *s2, int len);
int strcmp_case(const char *s1, const char *s2);

int get_int_method(char *s);
char *get_str_method(int i);

int get_int_http_prot(char *s);
char *get_str_http_prot(int i);

char *strstr_lowercase(const char *s, const char *key);
int clean_path(char *path);

const char *content_type(const wchar_t *path);
int parse_startline_request(request *req, char *s, int len);
int parse_headers(request *req, char *s, int len);
string hex_dump(void *p, int n);
void path_correct(wstring& path);
//----------------------- multibytes -----------------------------------
int utf16_to_mbs(string& s, const wchar_t *ws);
int mbs_to_utf16(wstring& ws, const char *u8);
int utf16_to_utf8(string& s, std::wstring& ws);
int utf16_to_utf8(string& s, const wchar_t *ws);
int utf8_to_utf16(char *u8, std::wstring& ws);
int utf8_to_utf16(string& u8, std::wstring& ws);
//-------------------- send_resp ---------------------------------------
void send_message(request *req, const char *msg);
int create_multipart_head(char *buf, request *req, struct Range *ranges, int len_buf);
char *create_header(request *req, const char *name, const char *val);
int send_header_response(request *req);
//----------------------------------------------------------------------
int read_timeout(SOCKET sock, char *buf, int len, int timeout);
int write_timeout(SOCKET sock, const char *buf, size_t len, int timeout);
int ReadFromPipe(PIPENAMED *Pipe, char *buf, int sizeBuf, int *allRD, int maxRd, int timeout);
int WriteToPipe(PIPENAMED *Pipe, const char *buf, int lenBuf, int maxRd, int timeout);
long long client_to_script(SOCKET sock, PIPENAMED *Pipe, long long cont_len, int sizePipeBuf, int timeout);
int send_file_1(SOCKET sock, int fd_in, char *buf, int *size, long long offset, long long *cont_len);
int send_file_2(SOCKET sock, int fd_in, char *buf, int size, long long offset);
int read_line_sock(SOCKET sock, char *buf, int size, int timeout);
int read_headers(request *req, int timeout1, int timeout2);
//----------------------------------------------------------------------
HANDLE open_logfiles(HANDLE, HANDLE);
void print_err(const char *format, ...);
void print_log(request *req);
//----------------------------------------------------------------------
void send_files(RequestManager *ReqMan);
void push_resp_queue(request *res);
void close_conv(void);

#endif