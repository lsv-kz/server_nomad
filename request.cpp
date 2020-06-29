#include "main.h"

/*====================================================================*/
void get_request(RequestManager *ReqMan)
{
	int readFromClient;
	int num_thr, num_req, n;
	char *p;
	request *req;
	int numChld = ReqMan->get_num_chld();

	while(1)
	{
		req = ReqMan->pop_req();
		if (!req)
		{
			print_err("%d<%s:%d>  req = NULL\n", numChld, __func__, __LINE__);
			ReqMan->exit_thr();
			return;
		}
		else if (req->clientSocket == INVALID_SOCKET) 
		{
			print_err("%d<%s:%d>  req->clientSocket == INVALID_SOCKET\n", numChld, __func__, __LINE__);
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
				req->req_hdrs.Value[req->req_hdrs.iReferer] = (char*)"Connection reset by peer";
				req->err = -1;
			}
			else if (readFromClient == -1000)
			{
				req->req_hdrs.iReferer = NUM_HEADERS - 1;
				req->req_hdrs.Value[req->req_hdrs.iReferer] = (char*)"Timeout";
				req->err = -1;
			}
			else
				req->err = readFromClient;

			goto end;
		}
		/*--------------------------------------------------------*/
		if ((req->httpProt == HTTP09) || (req->httpProt == HTTP2))
		{
			req->connKeepAlive = 0;
			req->err = -RS505;
			goto end;
		}

		if (req->numReq >= (unsigned int)conf->MaxRequestsPerThr || (conf->KeepAlive == 'n') || (req->httpProt == HTTP10))
			req->connKeepAlive = 0;
		else if (req->req_hdrs.iConnection == -1)
			req->connKeepAlive = 1;

		if ((p = strrchr(req->uri, '?')))
		{
			req->sReqParam = p + 1;
			*p = '\0';
			req->uriLen = strlen(req->uri);
			*p = '?';
		}
		else
		{
			if ((p = strstr_case(req->uri, "%3F")))
			{
				req->sReqParam = p + 3;
				*p = '\0';
				req->uriLen = strlen(req->uri);
				*p = '%';
			}
			else
			{
				req->sReqParam = NULL;
				req->uriLen = strlen(req->uri);
			}
		}

		decode(req->uri, req->uriLen, req->decodeUri, sizeof(req->decodeUri) - 1);
		clean_path(req->decodeUri);
		//--------------------------------------------------------------
		n = utf8_to_utf16(req->decodeUri, req->wDecodeUri);
		if (n)
		{
			print_err("<%s:%d> utf8_to_utf16()=%d\n", __func__, __LINE__, n);
			req->err = -RS500;
			goto end;
		}

		req->lenDecodeUri = strlen(req->decodeUri);
		//--------------------------------------------------------------
		if ((req->reqMethod == M_GET) || (req->reqMethod == M_HEAD) || (req->reqMethod == M_POST))
		{
			int ret = response(ReqMan, req);
			// "req" may be free !!!
			if (ret == 1)
			{
				int ret = ReqMan->end_req(&num_thr, &num_req);
				if (ret == EXIT_THR)
					return;
				else
					continue;
			}
			
			req->free_resp_headers();
			req->free_range();
			req->err = ret;
		}
		else if (req->reqMethod == M_OPTIONS)
		{
			req->err = options(req);
			req->free_resp_headers();
		}
		else
			req->err = -RS501;

	end:
		if (req->err <= -RS101)
		{
			req->resp.respStatus = -req->err;
			send_message(req, "");

			if ((req->reqMethod == M_POST) || (req->reqMethod == M_PUT))
				req->connKeepAlive = 0;
		}

		ReqMan->close_response(req);
		
		int ret = ReqMan->end_req(&num_thr, &num_req);
		if (ret)
		{
			return;
		}
	}
}
//======================================================================
int options(request *req)
{
	req->resp.respStatus = RS204;
	send_header_response(req);
	return 0;
}