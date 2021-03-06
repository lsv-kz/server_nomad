#include "main.h"

using namespace std;

/*====================================================================*/
void get_request(RequestManager* ReqMan)
{
    int readFromClient;
    int num_thr, num_req, n;
    const char* p;
    Connect* req;
    int numChld = ReqMan->get_num_chld();

    while (1)
    {
        req = ReqMan->pop_req();
        if (!req)
        {
            print_err("[%d]<%s:%d>  req = NULL\n", numChld, __func__, __LINE__);
            ReqMan->exit_thr();
            return;
        }
        else if (req->clientSocket == INVALID_SOCKET)
        {
            print_err("[%d]<%s:%d>  req->clientSocket == INVALID_SOCKET\n", numChld, __func__, __LINE__);
            ReqMan->exit_thr();
            delete req;
            return;
        }

        req->init();
        /*--------------------- read_request ---------------------*/
        readFromClient = read_headers(req, req->timeout, conf->TimeOut);
        req->timeout = conf->TimeoutKeepAlive;
        if (readFromClient <= 0)
        {
            if (readFromClient == 0)
            {
                req->req_hdrs.iReferer = NUM_HEADERS - 1;
                req->req_hdrs.Value[req->req_hdrs.iReferer] = "Connection reset by peer";
                req->err = -1;
            }
            else if (readFromClient == -1000)
            {
                req->req_hdrs.iReferer = NUM_HEADERS - 1;
                req->req_hdrs.Value[req->req_hdrs.iReferer] = "Timeout";
                req->err = -1;
            }
            else
                req->err = readFromClient;

            goto end;
        }
        /*--------------------------------------------------------*/
        if ((req->httpProt != HTTP10) && (req->httpProt != HTTP11))
        {
            req->httpProt = HTTP11;
            req->connKeepAlive = 0;
            req->err = -RS505;
            goto end;
        }

        if (req->numReq >= (unsigned int)conf->MaxRequestsPerThr || (conf->KeepAlive == 'n') || (req->httpProt == HTTP10))
            req->connKeepAlive = 0;
        else if (req->req_hdrs.iConnection == -1)
            req->connKeepAlive = 1;

        if ((p = strchr(req->uri, '?')))
        {
            req->uriLen = p - req->uri;
            req->sReqParam = req->uri + req->uriLen + 1;
        }
        else
        {
            if ((p = strstr_case(req->uri, "%3F")))
            {
                req->uriLen = p - req->uri;
                req->sReqParam = req->uri + req->uriLen + 3;
            }
            else
            {
                req->sReqParam = "";
                req->uriLen = strlen(req->uri);
            }
        }

        decode(req->uri, req->uriLen, req->decodeUri, sizeof(req->decodeUri) - 1);
        clean_path(req->decodeUri);
        //--------------------------------------------------------------
        n = utf8_to_utf16(req->decodeUri, req->wDecodeUri);
        if (n)
        {
            print_err(req, "<%s:%d> utf8_to_utf16()=%d\n", __func__, __LINE__, n);
            req->err = -RS500;
            goto end;
        }
        //--------------------------------------------------------------
        if ((req->reqMethod == M_GET) || (req->reqMethod == M_HEAD) || (req->reqMethod == M_POST))
        {
            int ret = response(ReqMan, req);
            if (ret == 1) // "req" may be free !!!
            {
                int ret = ReqMan->end_thr(&num_thr, &num_req);
                if (ret == EXIT_THR)
                    return;
                else
                    continue;
            }
 
            req->err = ret;
        }
        else if (req->reqMethod == M_OPTIONS)
        {
            req->err = options(req);
        }
        else
            req->err = -RS501;

    end:
        if (req->err <= -RS101)
        {
            req->resp.respStatus = -req->err;
            send_message(req, "", NULL);

            if (req->reqMethod == M_POST)
                req->connKeepAlive = 0;
        }

        ReqMan->end_response(req);

        int ret = ReqMan->end_thr(&num_thr, &num_req);
        if (ret)
        {
            return;
        }
    }
}
//======================================================================
int options(Connect* req)
{
    req->resp.respStatus = RS204;
    if (send_response_headers(req, NULL) < 0)
        return -1;
    return 0;
}
