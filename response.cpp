#include "main.h"

using namespace std;

int send_multy_part(Connect* req, int fi, char* rd_buf, int* size);

const char boundary[] = "----------a9b5r7a4c0a2d5a1b8r3a";
//======================================================================
long long file_size(const wchar_t* s)
{
    struct _stati64 st;

    if (!_wstati64(s, &st))
        return st.st_size;
    else
        return -1;
}
//======================================================================
int response(RequestManager* ReqMan, Connect* req)
{
    if ((strstr(req->decodeUri, ".php")))
    {
        int ret;
        if ((conf->usePHP != "php-cgi") && (conf->usePHP != "php-fpm"))
        {
            print_err(req, "<%s:%d> Not found: %s\n", __func__, __LINE__, req->decodeUri);
            return -RS404;
        }
        struct _stat st;
        if (_wstat(req->wDecodeUri.c_str() + 1, &st) == -1)
        {
            print_err(req, "<%s:%d> script (%s) not found\n", __func__, __LINE__, req->decodeUri);
            return -RS404;
        }

        wstring s = req->wDecodeUri;
        req->wScriptName = s.c_str();
        if (conf->usePHP == "php-cgi")
        {
            req->resp.scriptType = php_cgi;
            ret = cgi(req);
            req->wScriptName = NULL;
            return ret;
        }
        else if (conf->usePHP == "php-fpm")
        {
            req->resp.scriptType = php_fpm;
            ret = fcgi(req);
            req->wScriptName = NULL;
            return ret;
        }
    }

    if (!strncmp(req->decodeUri, "/cgi-bin/", 9)
        || !strncmp(req->decodeUri, "/cgi/", 5))
    {
        req->resp.scriptType = cgi_ex;
        wstring s = req->wDecodeUri;
        req->wScriptName = s.c_str();

        int ret = cgi(req);
        req->wScriptName = NULL;
        return ret;
    }
    //-------------------------- get path ------------------------------
    wstring wPath;
    wPath.reserve(conf->wRootDir.size() + req->wDecodeUri.size() + 256);
    wPath += conf->wRootDir;
    wPath += req->wDecodeUri;
    if (wPath[wPath.size() - 1] == L'/')
        wPath.resize(wPath.size() - 1);
    //------------------------------------------------------------------
    struct _stati64 st64;
    if (_wstati64(wPath.c_str(), &st64) == -1)
    {
        string sTmp;
        utf16_to_utf8(sTmp, wPath);
        print_err(req, "<%s:%d> Error _wstati64(%s): %d\n", __func__, __LINE__, sTmp.c_str(), errno);
        return -RS404;
    }
    else
    {
        if ((!(st64.st_mode & _S_IFDIR)) && (!(st64.st_mode & _S_IFREG)))
        {
            print_err(req, "<%s:%d> Error: file (!S_ISDIR && !S_ISREG) \n", __func__, __LINE__);
            return -RS403;
        }
    }
    //------------------------------------------------------------------
    if (st64.st_mode & _S_IFDIR)
    {
        if (req->uri[req->uriLen - 1] != '/')
        {
            const char* p = "The document has moved <a href=\"%s\">here</a>";
            char* s;
            req->uri[req->uriLen] = '/';
            req->uri[req->uriLen + 1] = '\0';
            req->resp.respStatus = RS301;

            if (!create_header(req, "Location: ", req->uri))
            {
                return -RS500;
            }

            size_t len = strlen(p) + req->uriLen + 2;

            s = new(nothrow) char[len];
            if (!s)
            {
                return -1;
            }

            snprintf(s, len, p, req->uri);
            send_message(req, s);
            delete[] s;
            return 0;
        }
        //--------------------------------------------------------------
        size_t len = wPath.size();
        wPath += L"/index.html";
        struct _stat st;
        if ((_wstat(wPath.c_str(), &st) == -1) || (conf->index_html != 'y'))
        {
            wPath.resize(len);
            return index_dir(ReqMan, req, wPath);
        }
    }
    //----------------------- send file --------------------------------
    req->resp.fileSize = file_size(wPath.c_str());
    req->resp.numPart = 0;
    snprintf(req->resp.respContentType, sizeof(req->resp.respContentType), "%s", content_type(wPath.c_str()));
    if (req->sRange[0])
    {
        int n = parse_range(req);
        if (n > 1)
        {
            req->resp.respStatus = RS206;
            if (n > 1)
                snprintf(req->resp.respContentType, sizeof(req->resp.respContentType),
                    "multipart/byteranges; boundary=%s", boundary);
        }
        else if (n == 1)
        {
            req->resp.respStatus = RS206;
        }
        else if (n == -RS416)
        {
            print_err(req, "<%s:%d> Error parse_range(%s); err=416\n", __func__, __LINE__, req->sRange);
            req->resp.numPart = 0;
            if (req->req_hdrs.iRange >= 0) req->resp.respStatus = RS200;
            else return n;
        }
        else
        {
            req->resp.respStatus = RS200;
        }
    }
    else
    {
        req->resp.respStatus = RS200;
        req->resp.offset = 0;
        req->resp.respContentLength = req->resp.fileSize;
    }
    //------------------------------------------------------------------
    if (_wsopen_s(&req->resp.fd, wPath.c_str(), _O_RDONLY | _O_BINARY, _SH_DENYWR, _S_IREAD))
    {
        string sTmp;
        utf16_to_utf8(sTmp, wPath);
        print_err(req, "<%s:%d> Error _wopen(%s); err=%d\n", __func__, __LINE__, sTmp.c_str(), errno);
        if (errno == EACCES)
            return -RS403;
        else
            return -RS500;
    }

    if (send_header_response(req) <= 0)
    {
        print_err(req, "<%s:%d>  Error send_header_response()\n", __func__, __LINE__);
        _close(req->resp.fd);
        return -1;
    }

    if ((req->reqMethod == M_HEAD) || (req->resp.respContentLength == 0))
    {
        _close(req->resp.fd);
        return 0;
    }

    wPath.clear();
    wPath.reserve(0);

    if (req->resp.numPart > 1)
    {
        char* rd_buf;
        int size = conf->SOCK_BUFSIZE;

        rd_buf = new(nothrow) char[size];
        if (!rd_buf)
        {
            print_err(req, "<%s:%d> Error malloc()\n", __func__, __LINE__);
            _close(req->resp.fd);
            return -1;
        }

        int ret = send_multy_part(req, req->resp.fd, rd_buf, &size);
        delete[] rd_buf;
        _close(req->resp.fd);
        return ret;
    }

    req->free_range();
    push_resp_queue(req);
    return 1;
}
//======================================================================
int send_multy_part(Connect* req, int fd, char* rd_buf, int* size)
{
    int n;
    long long send_all_bytes, len;
    char buf[1024];
    //------------------------------------------------------------------
    long long all_bytes = 0;
    int i;
    struct Range* range;

    send_all_bytes = 0;

    for (i = 0; i < req->resp.numPart; i++)
    {
        all_bytes += req->resp.rangeBytes[i].part_len;
    }

    wstring path;
    (path += conf->wRootDir) += req->wDecodeUri;
    snprintf(req->resp.respContentType, sizeof(req->resp.respContentType), "%s",
        content_type(path.c_str()));

    for (i = 0; i < req->resp.numPart; i++)
    {
        range = &req->resp.rangeBytes[i];
        if ((n = create_multipart_head(buf, req, range, sizeof(buf))))
        {
            print_err(req, "<%s:%d> Error create_multipart_head()=%d\n", __func__, __LINE__, n);
            return -1;
        }

        n = write_timeout(req->clientSocket, buf, strlen(buf), conf->TimeOut);
        if (n < 0)
        {
            print_err(req, "<%s:%d> Error: write_timeout(), %lld bytes from %lld bytes\n",
                 __func__, __LINE__, send_all_bytes, all_bytes);
            return -1;
        }

        len = range->part_len;
        n = send_file_1(req->clientSocket, fd, rd_buf, size, range->start, &range->part_len);
        if (n < 0)
        {
            print_err(req, "<%s:%d> Error: Sent %lld bytes from %lld bytes; fd=%d\n",
                    __func__, __LINE__,
                send_all_bytes += (len - range->part_len), all_bytes, req->clientSocket);
            return -1;
        }
        else
            send_all_bytes += (len - range->part_len);

        snprintf(buf, sizeof(buf), "%s", "\r\n");
        n = write_timeout(req->clientSocket, buf, 2, conf->TimeOut);
        if (n < 0)
        {
            print_err(req, "<%s:%d> Error: write_timeout() %lld bytes from %lld bytes\n",
                    __func__, __LINE__, send_all_bytes, all_bytes);
            return -1;
        }
    }

    snprintf(buf, sizeof(buf), "--%s--\r\n", boundary);
    n = write_timeout(req->clientSocket, buf, strlen(buf), conf->TimeOut);
    req->resp.send_bytes = send_all_bytes;
    if (n < 0)
    {
        print_err(req, "<%s:%d> Error: write_timeout() %lld bytes from %lld bytes\n",
                __func__, __LINE__, send_all_bytes, all_bytes);
        return -1;
    }

    return 0;
}
/*====================================================================*/
const char* status_resp(int st)
{
    switch (st)
    {
    case 0:
        return "";
    case RS101:
        return "101 Switching Protocols";
    case RS200:
        return "200 OK";
    case RS204:
        return "204 No Content";
    case RS206:
        return "206 Partial Content";
    case RS301:
        return "301 Moved Permanently";
    case RS302:
        return "302 Moved Temporarily";
    case RS400:
        return "400 Bad Request";
    case RS401:
        return "401 Unauthorized";
    case RS402:
        return "402 Payment Required";
    case RS403:
        return "403 Forbidden";
    case RS404:
        return "404 Not Found";
    case RS405:
        return "405 Method Not Allowed";
    case RS406:
        return "406 Not Acceptable";
    case RS407:
        return "407 Proxy Authentication Required";
    case RS408:
        return "408 Request Timeout";
    case RS411:
        return "411 Length Required";
    case RS413:
        return "413 Request entity too large";
    case RS414:
        return "414 Request-URI Too Large";
    case RS416:
        return "416 Range Not Satisfiable";
    case RS418:
        return "418 I'm a teapot";
    case RS500:
        return "500 Internal Server Error";
    case RS501:
        return "501 Not Implemented";
    case RS502:
        return "502 Bad Gateway";
    case RS503:
        return "503 Service Unavailable";
    case RS504:
        return "504 Gateway Time-out";
    case RS505:
        return "505 HTTP Version not supported";
    default:
        return "";
    }
    return "";
}
//========================== send_message ==============================
void send_message(Connect* req, const char* msg)
{
    ostringstream html;

    if ((req->resp.respStatus != RS204) && (req->reqMethod != M_HEAD))
    {
        const char* title = status_resp(req->resp.respStatus);
        html << "<!DOCTYPE html>\n"
            "<html>\n"
            "<head>\n"
            "<title>" << title
            << "</title>\n"
            "<meta charset=\"utf-8\">\n"
            "</head>\n"
            "<body>\n"
            "<h3>" << title
            << "</h3>\n"
            "<p>" << (msg ? msg : "")
            << "</p>\n"
            "<hr>\n"
            << req->resp.sLogTime
            << "\n"
            "</body>\n"
            "</html>";

        memcpy(req->resp.respContentType, (char*)"text/html", 10);
        req->resp.respContentLength = html.str().size();
    }
    else
    {
        req->resp.respContentLength = 0;
        req->resp.respContentType[0] = 0;
    }

    if ((send_header_response(req) > 0) && (req->resp.respContentLength > 0))
    {
        req->resp.send_bytes = write_timeout(req->clientSocket, html.str().c_str(), (size_t)req->resp.respContentLength, conf->TimeOut);
        if (req->resp.send_bytes <= 0)
        {
            print_err(req, "<%s:%d> Error write_timeout()\n", __func__, __LINE__);
            req->connKeepAlive = 0;
        }
    }
}
/*====================================================================*/
int create_multipart_head(char* buf, Connect* req, struct Range* ranges, int len_buf)
{
    int n;

    n = snprintf(buf, len_buf, "--%s\r\n", boundary);
    buf += n;
    len_buf -= n;

    if (req->resp.respContentType[0] && (len_buf > 0))
    {
        n = snprintf(buf, len_buf, "Content-Type: %s\r\n", req->resp.respContentType);
        buf += n;
        len_buf -= n;
    }
    else
        return 2;

    if ((req->resp.numPart > 1) && (len_buf > 0))
    {
        n = snprintf(buf + strlen(buf), len_buf - strlen(buf),
            "Content-Range: bytes %I64d-%I64d/%I64d\r\n\r\n",
            ranges->start, ranges->end, req->resp.fileSize);
        buf += n;
        len_buf -= n;
    }
    else
        return 3;

    return 0;
}
/*====================================================================*/
char* create_header(Connect* req, const char* name, const char* val)
{
    if (name)
    {
        size_t lenName = strlen(name), lenVal = 0;
        char* p;
        if (val)
            lenVal = strlen(val);

        p = new(nothrow) char[lenName + lenVal + 1];
        if (!p)
            return NULL;

        memcpy(p, name, lenName + 1);
        if (val)
            memcpy(p + lenName, val, lenVal + 1);
        req->resp.respHeaders[req->resp.countRespHeaders] = p;
        req->resp.respHeaders[++req->resp.countRespHeaders] = NULL;
        return p;
    }

    return NULL;
}
/*====================================================================*/
int send_header_response(Connect* req)
{
    ostringstream ss;

    if (req->httpProt == HTTP09)
        return 1;
    else if ((req->httpProt == HTTP2) || (req->httpProt == 0))
        req->httpProt = HTTP11;

    ss << get_str_http_prot(req->httpProt) << " " << status_resp(req->resp.respStatus) << "\r\n"
        << "Date: " << req->resp.sLogTime << "\r\n"
        << "Server: " << conf->ServerSoftware << "\r\n";

    if (req->reqMethod == M_OPTIONS)
        ss << "Allow: OPTIONS, GET, HEAD, POST\r\n";
    else
        ss << "Accept-Ranges: bytes\r\n";

    if ((req->resp.numPart == 1) && req->resp.rangeBytes)
    {
        ss << "Content-Range: bytes " << req->resp.rangeBytes[0].start << "-" << req->resp.rangeBytes[0].end << "/" << req->resp.fileSize << "\r\n";
        ss << "Content-Length: " << req->resp.rangeBytes[0].part_len << "\r\n";
    }
    else
    {
        if ((req->resp.numPart <= 1) && (req->resp.respContentLength != -1))
        {
            ss << "Content-Length: " << req->resp.respContentLength << "\r\n";
        }
    }

    if (req->resp.respContentType[0])
    {
        ss << "Content-Type: " << req->resp.respContentType << "\r\n";
        //print_err(req, "<%s:%d> %s\n", __func__, __LINE__, req->respContentType);
    }

    if (req->resp.respStatus == RS101)
    {
        ss << "Upgrade: HTTP/1.1\r\n"
            << "Connection: Upgrade\r\n";
    }
    else
    {
        ss << "Connection: " << (req->connKeepAlive == 0 ? "close" : "keep-alive") << "\r\n";
    }
    /*----------------------------------------------------------------*/
    for (int i = 0; req->resp.respHeaders[i]; i++)
    {
        ss << req->resp.respHeaders[i] << "\r\n";
        //print_err(req, "<%s:%d> %s\n", __func__, __LINE__, req->respHeaders[i]);
    }

    if (req->resp.numPart > 1)
    {
        ss << "\r\n\r\n";
    }
    else
    {
        ss << "\r\n";
    }

    int len = (int)ss.str().size();
    int n = write_timeout(req->clientSocket, ss.str().c_str(), len, conf->TimeOut);
    req->free_resp_headers();
    if (n <= 0)
    {
        print_err("%d<%s:%d> Sent to client response error; (%d)\n", req->numChld, __func__, __LINE__, n);
        return n;
    }

    return n;
}

