#include "main.h"

request **resp_queue;
fd_set wrfds;
int size_queue = 0;

mutex mtx_send;
condition_variable cond_add;
condition_variable cond_shift;
int count_resp = 0;
int close_thr = 0;
/*====================================================================*/
int send_entity(request *req, char *rd_buf, int size_buf)
{
	int ret;
	int len;
	if (req->resp.respContentLength >= (long long)size_buf)
		len = size_buf;
	else
	{
		len = (int)req->resp.respContentLength;
		if (len == 0)
			return 0;
	}
	
	ret = send_file_2(req->clientSocket, req->resp.fd, rd_buf, len, req->resp.offset);
	if(ret <= 0)
	{
		if (ret == -1)
			print_err("%d<%s:%d> Error: Sent %lld bytes\n", req->numChld, __func__, __LINE__, req->resp.send_bytes);
		return ret;
	}
		
	req->resp.send_bytes += ret;
	req->resp.offset += ret;
	req->resp.respContentLength -= ret;
	if (req->resp.respContentLength == 0)
		ret = 0;
 
	return ret;
}
//======================================================================
int shift_queue()
{
	int i = 0, i_empty = 0;
	time_t t = time(NULL);
	
	while (i < size_queue)
	{
		if (resp_queue[i] != NULL)
		{
			if (resp_queue[i]->time_write == 0)
				resp_queue[i]->time_write = t;
			FD_SET(resp_queue[i]->clientSocket, &wrfds);
			if (i > i_empty)
			{
				resp_queue[i_empty] = resp_queue[i];
				resp_queue[i] = NULL;
			}

			++i_empty;
		}
		
		++i;
	}
	size_queue = i_empty;
	return 0;
}
//======================================================================
void delete_request(int i, RequestManager *ReqMan)
{
	_close(resp_queue[i]->resp.fd);
	ReqMan->close_response(resp_queue[i]);
mtx_send.lock();
	resp_queue[i] = NULL;
	--count_resp;
mtx_send.unlock();
}
//======================================================================
void delete_timeout_requests(int n, RequestManager *ReqMan)
{
	int i = 0;
	time_t t = time(NULL);
	
	while (i < n)
	{
		if (resp_queue[i] != NULL)
		{
			if ((t - resp_queue[i]->time_write) > conf->TimeOut)
			{
				print_err("%d<%s:%d> Timeout = %ld\n", resp_queue[i]->numChld, __func__, __LINE__, t - resp_queue[i]->time_write);
				_close(resp_queue[i]->resp.fd);
				resp_queue[i]->req_hdrs.iReferer = NUM_HEADERS - 1;
				resp_queue[i]->req_hdrs.Value[resp_queue[i]->req_hdrs.iReferer] = (char*)"Timeout";
				ReqMan->close_connect(resp_queue[i]);
		mtx_send.lock();
				resp_queue[i] = NULL;
				--count_resp;
		mtx_send.unlock();
			}
		}
		++i;
	}
}
//======================================================================
void send_files(RequestManager *ReqMan)
{
	int i, ret = 0;
	int num_select, timeout = 1;
	int size_buf = conf->SOCK_BUFSIZE;
	time_t time_write;
	struct timeval tv;
	char *rd_buf;
	
	resp_queue = new(nothrow) request* [conf->SizeQueue];
	rd_buf = new(nothrow) char [size_buf];
	if (!resp_queue || !rd_buf)
	{
		print_err("%d<%s:%d> Error malloc(): %d\n", ReqMan->get_num_chld(), __func__, __LINE__, errno);
		exit(1);
	}
	
	FD_ZERO(&wrfds);
	
	i = 0;
	while (i < conf->SizeQueue)
	{
		resp_queue[i] = NULL;
		++i;
	}
	
	i = 0;
	while (1)
	{
		{
			unique_lock<mutex> lk(mtx_send);
			if (count_resp == 0)
				cond_add.wait(lk);

			if (close_thr)
				break;
			shift_queue();
			num_select = size_queue;
		}
		cond_shift.notify_one();
		
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		ret = select(0, NULL, &wrfds, NULL, &tv);
		if (ret == -1)
		{
			print_err("%d<%s:%d> Error select(): %d\n", ReqMan->get_num_chld(), __func__, __LINE__, WSAGetLastError());
			ret = 0;
			continue;
		}
		else if (ret == 0)
		{
			delete_timeout_requests(num_select, ReqMan);
			continue;
		}
		
		time_write = time(NULL);
		i = 0;
		while ((ret > 0) && (i < num_select))
		{
			if (resp_queue[i])
			{
				if (FD_ISSET(resp_queue[i]->clientSocket, &wrfds))
				{
					FD_CLR(resp_queue[i]->clientSocket, &wrfds);
					int wr = send_entity(resp_queue[i], rd_buf, size_buf);
					if (wr == 0)
					{
						resp_queue[i]->err = wr;
						delete_request(i, ReqMan);
					}
					else if (wr == -1)
					{
						resp_queue[i]->err = wr;
						resp_queue[i]->req_hdrs.iReferer = NUM_HEADERS - 1;
						resp_queue[i]->req_hdrs.Value[resp_queue[i]->req_hdrs.iReferer] = (char*)"Connection reset by peer";
						delete_request(i, ReqMan);
					}
					else // (wr > 0)
					{
						resp_queue[i]->time_write = 0;  
					}
					--ret;
				}
				else
				{
					time_t t = time_write - resp_queue[i]->time_write;
					if (t > conf->TimeOut)
					{
						print_err("%d<%s:%d> Timeout = %ld\n", ReqMan->get_num_chld(), __func__, __LINE__, t);
						resp_queue[i]->req_hdrs.iReferer = NUM_HEADERS - 1;
						resp_queue[i]->req_hdrs.Value[resp_queue[i]->req_hdrs.iReferer] = (char*)"Timeout";
						resp_queue[i]->err = -1;
						delete_request(i, ReqMan);
					}
				}
			}
			++i;
		}
	}
	
	delete [] rd_buf;
	delete [] resp_queue;
}
//======================================================================
void push_resp_queue(request *req)
{
	req->free_resp_headers();
	req->free_range();
unique_lock<mutex> lk(mtx_send);
	while ((size_queue >= conf->SizeQueue) || resp_queue[size_queue])
	{
		print_err("%d<%s:%d>  wait(); size_conv=%d\n", req->numChld, __func__, __LINE__, size_queue);
		cond_shift.wait(lk);
	}

	req->time_write = 0;
	resp_queue[size_queue] = req;
	++count_resp;
	++size_queue;
	
	cond_add.notify_one();
}
//======================================================================
void close_conv(void)
{
	close_thr = 1;
	cond_add.notify_one();
}
