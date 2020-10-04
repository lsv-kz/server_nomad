#ifndef CHUNK_H_
#define CHUNK_H_
#include "main.h"

using namespace std;

const int CHUNK_SIZE_BUF = 4096;
const int MAX_LEN_SIZE_CHUNK = 6;
//======================================================================
class ClChunked
{
    int i, mode, allSend;
    SOCKET sock;
    char buf[CHUNK_SIZE_BUF + MAX_LEN_SIZE_CHUNK + 10];
    ClChunked() {};
    //------------------------------------------------------------------
    int send_chunk(int size)
    {
        const char* p;
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
            memcpy(buf + MAX_LEN_SIZE_CHUNK + i, "\r\n", 2);
            i += 2;
            p = buf + n;
            len += i;
        }
        else
        {
            p = buf + MAX_LEN_SIZE_CHUNK;
            len = i;
        }

        int ret = write_timeout(sock, p, len, conf->TimeOut);

        i = 0;
        if (ret > 0)
            allSend += ret;
        return ret;
    }
public:
    //---------------------------------------------------------------
    ClChunked(SOCKET s, int m) { sock = s; mode = m; i = allSend = 0; }
    //------------------------------------------------------------------
    ClChunked& operator << (const long long ll)
    {
        ostringstream ss;
        ss << ll;
        int n = 0, len = ss.str().size();
        while (CHUNK_SIZE_BUF < (i + len))
        {
            int l = CHUNK_SIZE_BUF - i;
            memcpy(buf + MAX_LEN_SIZE_CHUNK + i, ss.str().c_str() + n, l);
            i += l;
            len -= l;
            n += l;
            int ret = send_chunk(i);
            if (ret < 0)
                throw ret;
        }

        memcpy(buf + MAX_LEN_SIZE_CHUNK + i, ss.str().c_str() + n, len);
        i += len;
        return *this;
    }
    //------------------------------------------------------------------
    ClChunked& operator << (const char* s)
    {
        if (!s) throw __LINE__;
        int n = 0, len = strlen(s);
        while (CHUNK_SIZE_BUF < (i + len))
        {
            int l = CHUNK_SIZE_BUF - i;
            memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s + n, l);
            i += l;
            len -= l;
            n += l;
            int ret = send_chunk(i);
            if (ret < 0)
                throw ret;
        }

        memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s + n, len);
        i += len;
        return *this;
    }
    //------------------------------------------------------------------
    ClChunked& operator << (const string& s)
    {
        int n = 0, len = s.size();
        if (len == 0) return *this;
        while (CHUNK_SIZE_BUF < (i + len))
        {
            int l = CHUNK_SIZE_BUF - i;
            memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s.c_str() + n, l);
            i += l;
            len -= l;
            n += l;
            int ret = send_chunk(i);
            if (ret < 0)
                throw ret;
        }
        memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s.c_str() + n, len);
        i += len;
        return *this;
    }
    //------------------------------------------------------------------
    int add_arr(const char* s, int len)
    {
        if (!s) return -1;
        int n = 0;
        while (CHUNK_SIZE_BUF < (i + len))
        {
            int l = CHUNK_SIZE_BUF - i;
            memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s + n, l);
            i += l;
            len -= l;
            n += l;
            int ret = send_chunk(i);
            if (ret < 0)
                return ret;
        }

        memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s + n, len);
        i += len;
        return 0;
    }
    //------------------------------------------------------------------
    int cgi_to_client(PIPENAMED * Pipe, int sizeBuf)
    {
        while (1)
        {
            if (CHUNK_SIZE_BUF <= i)
            {
                int ret = send_chunk(i);
                if (ret < 0)
                    return ret;
            }

            int size = CHUNK_SIZE_BUF - i, rd;
            int ret = ReadFromPipe(Pipe, buf + MAX_LEN_SIZE_CHUNK + i, size, &rd, sizeBuf, conf->TimeOutCGI);
            if (ret == 0)
            {
                print_err("<%s:%d> ret=%d, rd=%d\n", __func__, __LINE__, ret, rd);
                if (rd > 0)
                {
                    i += rd;
                }
                break;
            }
            else if (ret < 0)
            {
                i = 0;
                return ret;
            }
            else
            {
                i += rd;
                if (rd != size)
                {
                    print_err("<%s:%d> %d rd != size %d\n", __func__, __LINE__, rd, size);
                    break;
                }
            }
        }

        return 0;
    }
    //------------------------------------------------------------------
    int fcgi_to_client(SOCKET fcgi_sock, int len)
    {
        while (len > 0)
        {
            if (CHUNK_SIZE_BUF <= i)
            {
                int ret = send_chunk(i);
                if (ret < 0)
                    return ret;
            }

            int rd = (len < (CHUNK_SIZE_BUF - i)) ? len : (CHUNK_SIZE_BUF - i);
            int ret = read_timeout(fcgi_sock, buf + MAX_LEN_SIZE_CHUNK + i, rd, conf->TimeOutCGI);
            if (ret <= 0)
            {
                print_err("<%s:%d> ret=%d\n", __func__, __LINE__, ret);
                i = 0;
                return -1;
            }
            else if (ret != rd)
            {
                print_err("<%s:%d> ret != rd\n", __func__, __LINE__);
                i = 0;
                return -1;
            }

            i += ret;
            len -= ret;
        }

        return 0;
    }
    //------------------------------------------------------------------
    int end()
    {
        if (mode)
        {
            int n = i;
            const char* s = "\r\n0\r\n";
            int len = strlen(s);
            memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s, len);
            i += len;
            return send_chunk(n);
        }
        else
            return send_chunk(0);
    }
    //------------------------------------------------------------------
    int all() { return allSend; }
};

#endif
