#ifndef CLASSES_H_
#define CLASSES_H_

#include "main.h"

//======================================================================
struct Range {
    long long start;
    long long end;
    long long part_len;
};
//----------------------------------------------------------------------
class ArrayRanges // except
{
protected:
    const int ADDITION = 8;
    Range* range;
    unsigned int sizeBuf;
    unsigned int lenBuf;
    int numPart;
    long long sizeFile;

    int check_ranges();
    int parse_ranges(char* sRange, int sizeStr);

public:
    ArrayRanges(const ArrayRanges&) = delete;
    ArrayRanges()
    {
        sizeBuf = lenBuf = 0;
        range = NULL;
    }

    ~ArrayRanges()
    {
        if (range) delete[] range;
    }

    int resize(unsigned int n)
    {
        if (n <= lenBuf)
            return 1;
        Range * tmp = new(std::nothrow) Range[n];
        if (!tmp)
            return 1;
        for (unsigned int c = 0; c < lenBuf; ++c)
            tmp[c] = range[c];
        if (range)
            delete[] range;
        range = tmp;
        sizeBuf = n;
        return 0;
    }

    ArrayRanges & operator << (const Range & val)
    {
        if (lenBuf >= sizeBuf)
            if (resize(sizeBuf + ADDITION)) throw ENOMEM;
        range[lenBuf++] = val;
        return *this;
    }

    Range * get(unsigned int i)
    {
        if (i < lenBuf)
            return range + i;
        else
            return NULL;
    }

    int len() { return lenBuf; }
    int size() { return sizeBuf; }

    int create_ranges(char* s, int sizeStr, long long sz);
};
//===============================================================
const int CHUNK_SIZE_BUF = 4096;
const int MAX_LEN_SIZE_CHUNK = 6;
enum mode_chunk { NO_SEND = 0, SEND_NO_CHUNK, SEND_CHUNK };
//======================================================================
class ClChunked  // noexcept
{
    int i, mode, allSend;
    int err = 0;
    SOCKET sock;
    char buf[CHUNK_SIZE_BUF + MAX_LEN_SIZE_CHUNK + 10];
    //------------------------------------------------------------------
    int send_chunk(int size)
    {
        const char* p;
        int len;
        if (mode == SEND_CHUNK)
        {
            std::stringstream ss;
            ss << std::uppercase << std::hex << size << "\r\n" << std::dec;
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
    ClChunked() = delete;
    //---------------------------------------------------------------
    ClChunked(SOCKET s, int m) { sock = s; mode = m; i = allSend = 0; }
    //------------------------------------------------------------------
    ClChunked& operator << (const long long ll)
    {
        if (err) return *this;
        std::ostringstream ss;
        ss << ll;
        int n = 0, len = ss.str().size();
        if (mode == NO_SEND)
        {
            allSend += len;
            return *this;
        }

        while (CHUNK_SIZE_BUF < (i + len))
        {
            int l = CHUNK_SIZE_BUF - i;
            memcpy(buf + MAX_LEN_SIZE_CHUNK + i, ss.str().c_str() + n, l);
            i += l;
            len -= l;
            n += l;
            int ret = send_chunk(i);
            if (ret < 0)
            {
                err = 1;
                return *this;
            }
        }

        memcpy(buf + MAX_LEN_SIZE_CHUNK + i, ss.str().c_str() + n, len);
        i += len;
        return *this;
    }
    //------------------------------------------------------------------
    ClChunked& operator << (const char* s)
    {
        if (err || (!s)) return *this;
        int n = 0, len = strlen(s);
        if (mode == NO_SEND)
        {
            allSend += len;
            return *this;
        }

        while (CHUNK_SIZE_BUF < (i + len))
        {
            int l = CHUNK_SIZE_BUF - i;
            memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s + n, l);
            i += l;
            len -= l;
            n += l;
            int ret = send_chunk(i);
            if (ret < 0)
            {
                err = 1;
                return *this;
            }
        }

        memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s + n, len);
        i += len;
        return *this;
    }
    //------------------------------------------------------------------
    ClChunked& operator << (const String& s)
    {
        if (err) return *this;
        int n = 0, len = s.len();
        if (len == 0) return *this;
        if (mode == NO_SEND)
        {
            allSend += len;
            return *this;
        }

        while (CHUNK_SIZE_BUF < (i + len))
        {
            int l = CHUNK_SIZE_BUF - i;
            memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s.str() + n, l);
            i += l;
            len -= l;
            n += l;
            int ret = send_chunk(i);
            if (ret < 0)
            {
                err = 1;
                return *this;
            }
        }
        memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s.str() + n, len);
        i += len;
        return *this;
    }
    //------------------------------------------------------------------
    ClChunked& operator << (const std::string& s)
    {
        if (err) return *this;
        int n = 0, len = s.size();
        if (len == 0) return *this;
        if (mode == NO_SEND)
        {
            allSend += len;
            return *this;
        }
        
        while (CHUNK_SIZE_BUF < (i + len))
        {
            int l = CHUNK_SIZE_BUF - i;
            memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s.c_str() + n, l);
            i += l;
            len -= l;
            n += l;
            int ret = send_chunk(i);
            if (ret < 0)
            {
                err = 1;
                return *this;
            }
        }
        memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s.c_str() + n, len);
        i += len;
        return *this;
    }
    //------------------------------------------------------------------
    int add_arr(const char* s, int len)
    {
        if (!s || err) return -1;
        if (mode == NO_SEND)
        {
            allSend += len;
            return 0;
        }

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
    int cgi_to_client(PIPENAMED* Pipe, int sizeBuf)
    {
        if (err) return -1;
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
            if (ret == 0) // BROKEN_PIPE
            {
          //      print_err("<%s:%d> ret=%d, rd=%d\n", __func__, __LINE__, ret, rd);
                if (rd > 0)
                {
                    i += rd;
                }
                break;
            }
            else if (ret < 0) // ERROR
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
                }
            }
        }

        return 0;
    }
    //------------------------------------------------------------------
    int fcgi_to_client(SOCKET fcgi_sock, int len)
    {
        if (err) return -1;
        if (mode == NO_SEND)
        {
            char buf[1024];
            allSend += len;
            read_timeout(fcgi_sock, buf, len, conf->TimeOutCGI);
            return 0;
        }

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
        if (err) return -1;
        if (mode == SEND_CHUNK)
        {
            int n = i;
            const char* s = "\r\n0\r\n";
            int len = strlen(s);
            memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s, len);
            i += len;
            return send_chunk(n);
        }
        else if (mode == SEND_NO_CHUNK)
            return send_chunk(0);
        else
            return 0;
    }
    //------------------------------------------------------------------
    int all() { return allSend; }
    int error() { return err; }
};


#endif
