#ifndef CHUNK_H_
#define CHUNK_H_
#include "main.h"
 
const int CHUNK_SIZE_BUF = 4096;
const int MAX_LEN_SIZE_CHUNK = 6;
//======================================================================
class ClChunked
{
	int i = MAX_LEN_SIZE_CHUNK, mode, allSend = 0;
	SOCKET sock;
	char buf[CHUNK_SIZE_BUF + MAX_LEN_SIZE_CHUNK + 10];
	ClChunked() {};
	//------------------------------------------------------------------
	template <typename Arg>
	int add(Arg arg)
	{
		ostringstream ss;
		ss << arg;
		int n = 0, len = ss.str().size();
		while ((CHUNK_SIZE_BUF + MAX_LEN_SIZE_CHUNK) < (i + len))
		{
			int l = CHUNK_SIZE_BUF + MAX_LEN_SIZE_CHUNK - i;
			memcpy(buf + i, ss.str().c_str() + n, l);
			i += l;
			len -= l;
			n += l;
			int ret = send_chunk(i - MAX_LEN_SIZE_CHUNK);
			if (ret < 0)
				return ret;
		}

		memcpy(buf + i, ss.str().c_str() + n, len);
		i += len;
		return 0;
	}
	//------------------------------------------------------------------
	int send_chunk(int size)
	{
		const char *p;
		int len;
		if (mode)
		{
			stringstream ss;
			ss << uppercase << hex << size << "\r\n" << dec;
			len = ss.str().size();
			int n = MAX_LEN_SIZE_CHUNK - len;
			if (n < 0)
				return -1;
			memcpy(buf + n, ss.str().c_str(), len);
			memcpy(buf + i, "\r\n", 2);
			i += 2;
			p = buf + n;
			len = i - n;
		}
		else
		{
			p = buf + MAX_LEN_SIZE_CHUNK;
			len = i - MAX_LEN_SIZE_CHUNK;
		}

		int ret = write_timeout(sock, p, len, conf->TimeOut);

		i = MAX_LEN_SIZE_CHUNK;
		if (ret > 0)
			allSend += ret;
		return ret;
	}
public://---------------------------------------------------------------
	ClChunked(SOCKET s, int m){sock = s; mode = m;}
	//------------------------------------------------------------------
	void add_str(int& n){}

	template <typename Arg, typename ...Args>
	void add_str(int& ret, Arg arg, Args... args)
	{
		if ((ret = add(arg)) < 0)
			return;
		add_str(ret, args...);
	}
	//------------------------------------------------------------------
	int add_arr(const char *s, int len)
	{
		int n = 0;
		while ((CHUNK_SIZE_BUF + MAX_LEN_SIZE_CHUNK) < (i + len))
		{
			int l = CHUNK_SIZE_BUF + MAX_LEN_SIZE_CHUNK - i;
			memcpy(buf + i, s + n, l);
			i += l;
			len -= l;
			n += l;
			int ret = send_chunk(i - MAX_LEN_SIZE_CHUNK);
			if (ret < 0)
				return ret;
		}

		memcpy(buf + i, s + n, len);
		i += len;
		return 0;
	}
	//------------------------------------------------------------------
	int cgi_to_client(PIPENAMED *Pipe, int sizeBuf)
	{
		int allSend = 0;
		while (1)
		{
			if ((CHUNK_SIZE_BUF + MAX_LEN_SIZE_CHUNK - i) <= 0)
			{
				int ret = send_chunk(i - MAX_LEN_SIZE_CHUNK);
				if (ret < 0)
					return ret;
			}

			int size = CHUNK_SIZE_BUF + MAX_LEN_SIZE_CHUNK - i, rd;
			int ret = ReadFromPipe(Pipe, buf + i, size, &rd, sizeBuf, conf->TimeOutCGI);
			if (ret == 0)
			{
				print_err("<%s:%d> ret=%d, rd=%d\n", __func__, __LINE__, ret, rd);
				if (rd > 0)
				{
					i += rd;
					allSend += rd;
				}
				break;
			}
			else if (ret < 0)
			{
				i = MAX_LEN_SIZE_CHUNK;
				return ret;
			}
			else if (rd != size)
			{
				print_err("<%s:%d> %d rd != size %d\n", __func__, __LINE__, rd, size);
				i += rd;
				allSend += rd;
				break;
			}
			else
			{
				i += rd;
				allSend += rd;
			}
		}
		
		return allSend;
	}
	//------------------------------------------------------------------
	int fcgi_to_client(SOCKET fcgi_sock, int len)
	{
		int allSend = 0;
		while (len > 0)
		{
			if ((CHUNK_SIZE_BUF + MAX_LEN_SIZE_CHUNK - i) <= 0)
			{
				int ret = send_chunk(i - MAX_LEN_SIZE_CHUNK);
				if (ret < 0)
					return ret;
			}
			
			int rd = (len < (CHUNK_SIZE_BUF + MAX_LEN_SIZE_CHUNK - i)) ? len : (CHUNK_SIZE_BUF + MAX_LEN_SIZE_CHUNK - i);
			int ret = read_timeout(fcgi_sock, buf + i, rd, conf->TimeOutCGI);
			if (ret == 0)
			{
				print_err("<%s:%d> ret=%d\n", __func__, __LINE__, ret);
				i = MAX_LEN_SIZE_CHUNK;
				return -1;
			}
			else if (ret < 0)
			{
				i = MAX_LEN_SIZE_CHUNK;
				return ret;
			}
			else if (ret != rd)
			{
				print_err("<%s:%d> ret != rd\n", __func__, __LINE__);
				i = MAX_LEN_SIZE_CHUNK;
				return -1;
			}
			
			i += ret;
			allSend += ret;
			len -= ret;
		}
		
		return allSend;
	}
	//------------------------------------------------------------------
	int end()
	{
		if (mode)
		{
			int n = i - MAX_LEN_SIZE_CHUNK;
			const char *s = "\r\n0\r\n";
			int len = strlen(s);
			memcpy(buf + i, s, len);
			i += len;
			return send_chunk(n);
		}
		else
			return send_chunk(0);
	}
	//------------------------------------------------------------------
	int all(){return allSend;}
};

#endif
