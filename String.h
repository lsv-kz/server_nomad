#ifndef CLASS_STRING_H_
#define CLASS_STRING_H_
#include <iostream>
#include <cstring>


//======================================================================
/*template <typename T>
std::string int_to_str(T t, int base)
{
    if ((base != 10) && (base != 16))
        return "";
    const int size = 21;
    int cnt, minus = 0;
    char s[size];
    const char* byte_to_char = "FEDCBA9876543210123456789ABCDEF";

    if (base == 16)
        cnt = sizeof(t) * 2;
    else
    {
        cnt = size - 1;
        if (t < 0) minus = 1;
    }
    s[cnt] = 0;
    while (cnt > 0)
    {
        --cnt;
        if (base == 10)
        {
            s[cnt] = byte_to_char[15 + (t % 10)];
            t /= 10;
        }
        else
        {
            s[cnt] = byte_to_char[15 + (t & 0x0f)];
            t = t >> 4;
        }
        if (t == 0) break;
    }
    if (base == 10)
    {
        if (cnt <= 0) return "";
        if (minus) s[--cnt] = '-';
    }
    return s + cnt;
}*/
//---------------------------------------------------------------------
class BaseString
{
public:
    int b;
    BaseString(int n) { b = n; }
};

#define Hex BaseString(16)
#define Dec BaseString(10)
//----------------------------------------------------------------------
class String
{
protected:
    const int ADDITION = 128;
    int add = ADDITION;
    unsigned long lenBuf = 0;
    unsigned long sizeBuf = 0;
    int err = 0;
    int base = 10;
    int p_ = 0;

    char* ptr = NULL;

    void append(const String& s)
    {
        if ((s.lenBuf == 0) || err) return;
        if ((lenBuf + s.lenBuf) >= sizeBuf)
        {
            reserve(lenBuf + s.lenBuf + 1 + add);
            if (err) return;
        }
        memcpy(ptr + lenBuf, s.ptr, s.lenBuf);
        lenBuf += s.lenBuf;
    }

    void append(const char ch)
    {
        if (err) return;
        unsigned long len = 1;
        if ((lenBuf + len) >= sizeBuf)
        {
            reserve(lenBuf + len + 1 + add);
            if (err) return;
        }
        memcpy(ptr + lenBuf, &ch, len);
        lenBuf += len;
    }

    void append(const char* s)
    {
        if (!s || err) return;
        unsigned long len = strlen(s);
        if ((lenBuf + len) >= sizeBuf)
        {
            reserve(lenBuf + len + 1 + add);
            if (err) return;
        }
        memcpy(ptr + lenBuf, s, len);
        lenBuf += len;
    }

    void append(const std::string & s)
    {
        if ((s.size() == 0) || err) return;
        unsigned long len = s.size();
        if ((lenBuf + len) >= sizeBuf)
        {
            reserve(lenBuf + len + 1 + add);
            if (err) return;
        }
        memcpy(ptr + lenBuf, s.c_str(), len);
        lenBuf += len;
    }

public:
    String() { }

    String(int n)
    {
        if (n == 0) return;
        reserve(n);
    }

    String(const String & b)
    {
        sizeBuf = lenBuf = err = 0;
        ptr = NULL;
        append(b);
    }

    String(String && b) noexcept
    {
        if (b.err) return;
        ptr = b.ptr;
        p_ = b.p_;
        lenBuf = b.lenBuf;
        sizeBuf = b.sizeBuf;

        b.ptr = NULL;
        b.sizeBuf = b.lenBuf = 0;
    }

    ~String() { if (ptr) delete[] ptr; }
    //-----------------------------------------------------------------
    void reserve(unsigned int n)
    {
        if (err) return;
        if ((n <= sizeBuf) || (n == 0))
        {
            err = 1;
            return;
        }

        char* newBuf = new(std::nothrow) char[n];
        if (!newBuf)
        {
            err = ENOMEM;
            return;
        }

        if (ptr)
        {
            memcpy(newBuf, ptr, lenBuf);
            delete[] ptr;
        }

        sizeBuf = n;
        ptr = newBuf;
        *(ptr + lenBuf) = 0;
    }
    //------------------------------ = --------------------------------
    String& operator = (const String & b)
    {
        if (err) return *this;
        if (this != &b)
        {
            lenBuf = 0;
            append(b);
            p_ = 0;
        }
        return *this;
    }

    String& operator = (const char* s)
    {
        if (err) return *this;
        lenBuf = 0;
        append(s);
        p_ = 0;
        return *this;
    }

    String& operator = (const std::string & s)
    {
        if (err) return *this;
        lenBuf = 0;
        append(s);
        p_ = 0;
        return *this;
    }
    //----------------------------- << --------------------------------
    String& operator << (const String& b) { if (err) return *this; append(b); return *this; }
    String& operator << (const char ch) { if (err) return *this; append(ch); return *this; }
    String& operator << (const char* s) { if (err) return *this; append(s); return *this; }
    String& operator << (char* s) { if (err) return *this; append(s); return *this; }
    String& operator << (const std::string& s) { if (err) return *this; append(s); return *this; }
    
    template <typename T>
    String& operator << (T t)
    {
        if (err) return *this;
        const unsigned long size = 21;
        char s[size];
        int cnt, minus = 0;
        const char* byte_to_char = "FEDCBA9876543210123456789ABCDEF";
        if (base == 16)
            cnt = sizeof(t) * 2;
        else
        {
            cnt = size - 1;
            if (t < 0) minus = 1;
        }
        s[cnt] = 0;
        while (cnt > 0)
        {
            --cnt;
            if (base == 10)
            {
                s[cnt] = byte_to_char[15 + (t % 10)];
                t /= 10;
            }
            else
            {
                s[cnt] = byte_to_char[15 + (t & 0x0f)];
                t = t >> 4;
            }
            if (t == 0) break;
        }
        if (base == 10)
        {
            if (cnt <= 0)
            {
                err = 1;
                return *this;
            }
            if (minus) s[--cnt] = '-';
        }

        append(s + cnt);
        return *this;
    }
    //-----------------------------------------------------------------
    String append(const char* s, unsigned long n)
    {
        String str(n + 1);
        unsigned int len = strlen(s);
        if (n > len) n = len;
        memcpy(str.ptr, s, n);
        str.lenBuf = n;
        return str;
    }
    //-----------------------------------------------------------------
    String& operator << (BaseString b)
    {
        if (err) return *this;
        base = b.b;
        return *this;
    }

    const char* str()
    {
        if (err || (!ptr)) return "";
        *(ptr + lenBuf) = 0;
        return ptr;
    }

    void clear() { err = lenBuf = p_ = 0; }
    int error() { return err; }
    unsigned long len() { if (err) return 0; return lenBuf; }
    unsigned long size() { return sizeBuf; }
    //----------------------------- >> ---------------------------------
    String& operator >> (String& s)
    {
        if (err || (this == &s)) return *this;
        if (ptr[p_] == ' ')
            for (; (ptr[p_] == ' '); ++p_);
        char* p = (char*)memchr(ptr + p_, ' ', lenBuf);
        if (p)
        {
            s = append(ptr + p_, (p - (ptr + p_)));
            p_ += (p - (ptr + p_) + 1);
            if (ptr[p_] == ' ')
                for (; (ptr[p_] == ' '); ++p_);
        }
        else
        {
            s = str() + p_;
            p_ = lenBuf;
        }
        return *this;
    }
};

#endif