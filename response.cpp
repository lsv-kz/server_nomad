#include "classes.h"

using namespace std;

int send_multy_part(Connect* req, ArrayRanges& rg, int fi, char* rd_buf, int* size);
int create_multipart_head(Connect* req, Range* ranges, const char* contentType, char* buf, int len_buf);
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
            if (req->reqMethod == M_HEAD)
                return -RS405;
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
    DWORD attr = GetFileAttributesW(wPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        PrintError(__func__, __LINE__, "GetFileAttributesW");
        return -RS500;
    }

    if (attr & FILE_ATTRIBUTE_HIDDEN)
    {
        print_err(req, "<%s:%d> Hidden\n", __func__, __LINE__);
        return -RS404;
    }

    if (st64.st_mode & _S_IFDIR)
    {
        if (req->uri[req->uriLen - 1] != '/')
        {
            req->uri[req->uriLen] = '/';
            req->uri[req->uriLen + 1] = '\0';
            req->resp.respStatus = RS301;

            String hdrs(127);
            hdrs << "Location: " << req->uri << "\r\n";
            if (hdrs.error())
            {
                print_err(req, "<%s:%d> Error create_header()\n", __func__, __LINE__);
                return -RS500;
            }

            String s(256);
            s << "The document has moved <a href=\"" << req->uri << "\">here</a>";
            if (s.error())
            {
                print_err(req, "<%s:%d> Error create_header()\n", __func__, __LINE__);
                return -1;
            }

            send_message(req, s.str(), &hdrs);
            return 0;
        }
        //--------------------------------------------------------------
        size_t len = wPath.size();
        wPath += L"/index.html";
        struct _stat st;
        if ((_wstat(wPath.c_str(), &st) == -1) || (conf->index_html != 'y'))
        {
            wPath.resize(len);
            if ((conf->usePHP != "n") && (conf->index_php == 'y'))
            {
                wPath += L"/index.php";
                if (_wstat(wPath.c_str(), &st) == 0)
                {
                    int ret;
                    wstring s = req->wDecodeUri;
                    s += L"/index.php";
                    req->wScriptName = s.c_str();
                    if (conf->usePHP == "php-fpm")
                    {
                        req->resp.scriptType = php_fpm;
                        ret = fcgi(req);
                    }
                    else if (conf->usePHP == "php-cgi")
                    {
                        req->resp.scriptType = php_cgi;
                        ret = cgi(req);
                    }
                    else
                        ret = -1;

                    req->resp.scriptName = NULL;
                    return ret;
                }
                wPath.resize(len);
            }

            return index_dir(ReqMan, req, wPath);
        }
    }
    //----------------------- send file --------------------------------
    req->resp.fileSize = file_size(wPath.c_str());
    req->resp.numPart = 0;
    snprintf(req->resp.respContentType, sizeof(req->resp.respContentType), "%s", content_type(wPath.c_str()));
    ArrayRanges rg;
    if (req->req_hdrs.iRange >= 0)
    {
        try
        {
            req->resp.numPart = rg.create_ranges(req->sRange, sizeof(req->sRange), req->resp.fileSize);
        }
        catch (...)
        {
            print_err(req, "<%s:%d> Error create_ranges\n", __func__, __LINE__);
            return -1;
        }
            
        if (req->resp.numPart > 1)
        {
            if (req->reqMethod == M_HEAD)
                return -RS405;
            req->resp.respStatus = RS206;
        }
        else if (req->resp.numPart == 1)
        {
            req->resp.respStatus = RS206;
            Range* pr = rg.get(0);
            if (pr)
            {
                req->resp.offset = pr->start;
                req->resp.respContentLength = pr->part_len;
            }
        }
        else
        {
            return -RS416;
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

    wPath.clear();
    wPath.reserve(0);

    if (req->resp.numPart > 1)
    {
        int size = conf->SOCK_BUFSIZE;
        char* rd_buf = new(nothrow) char[size];
        if (!rd_buf)
        {
            print_err(req, "<%s:%d> Error malloc()\n", __func__, __LINE__);
            _close(req->resp.fd);
            return -1;
        }

        int ret = send_multy_part(req, rg, req->resp.fd, rd_buf, &size);
        delete[] rd_buf;
        _close(req->resp.fd);
        return ret;
    }

    if (send_response_headers(req, NULL))
    {
        print_err(req, "<%s:%d>  Error send_header_response()\n", __func__, __LINE__);
        _close(req->resp.fd);
        return -1;
    }

    if (req->reqMethod == M_HEAD)
    {
        _close(req->resp.fd);
        return 0;
    }

    push_resp_queue(req);
    return 1;
}
//======================================================================
int send_multy_part(Connect* req, ArrayRanges& rg, int fd, char* rd_buf, int* size)
{
    int n;
    long long send_all_bytes = 0, len;
    char buf[1024];
    //------------------------------------------------------------------
    long long all_bytes = 0;
    
    all_bytes += 2;
    Range* range;
    for (int i = 0; (range = rg.get(i)) && (i < req->resp.numPart); ++i)
    {
        all_bytes += (range->part_len + 2);
        all_bytes += create_multipart_head(req, range, req->resp.respContentType, buf, sizeof(buf));
    }
    all_bytes += snprintf(buf, sizeof(buf), "--%s--\r\n", boundary);
    req->resp.respContentLength = all_bytes;

    String hdrs(256);
    hdrs << "Content-Type: multipart/byteranges; boundary=" << boundary << "\r\n";
    hdrs << "Content-Length: " << all_bytes << "\r\n";
    if (hdrs.error())
    {
        print_err(req, "<%s:%d> Error create response headers\n", __func__, __LINE__);
        return -1;
    }
    
    if (send_response_headers(req, &hdrs))
    {
        return -1;
    }

    send_all_bytes += 2;
    
    for (int i = 0; (range = rg.get(i)) && (i < req->resp.numPart); ++i)
    {
        if ((n = create_multipart_head(req, range, req->resp.respContentType, buf, sizeof(buf))) == 0)
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
        send_all_bytes += n;

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
        send_all_bytes += n;
    }

    req->resp.send_bytes = send_all_bytes;
    snprintf(buf, sizeof(buf), "--%s--\r\n", boundary);
    n = write_timeout(req->clientSocket, buf, strlen(buf), conf->TimeOut);
    if (n < 0)
    {
        req->resp.send_bytes = send_all_bytes;
        print_err(req, "<%s:%d> Error: write_timeout() %lld bytes from %lld bytes\n",
                __func__, __LINE__, send_all_bytes, all_bytes);
        return -1;
    }
    req->resp.send_bytes += n;
    return 0;
}
/*====================================================================*/
int create_multipart_head(Connect* req, struct Range* ranges, const char* contentType, char* buf, int len_buf)
{
    int n, all = 0;

    n = snprintf(buf, len_buf, "--%s\r\n", boundary);
    buf += n;
    len_buf -= n;
    all += n;

    if (contentType && (len_buf > 0))
    {
        n = snprintf(buf, len_buf, "Content-Type: %s\r\n", contentType);
        buf += n;
        len_buf -= n;
        all += n;
    }
    else
        return 0;

    if (len_buf > 0)
    {
        n = snprintf(buf, len_buf,
            "Content-Range: bytes %lld-%lld/%lld\r\n\r\n",
            ranges->start, ranges->end, req->resp.fileSize);
        buf += n;
        len_buf -= n;
        all += n;
    }
    else
        return 0;

    return all;
}

