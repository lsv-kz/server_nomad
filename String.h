#ifndef CLASS_STRING_H_
#define CLASS_STRING_H_
#include <iostream>
#include <cstring>

//======================================================================
template <typename T>
int int_to_str(T t, char* buf, unsigned int sizeBuf, int base)
{
    if ((base != 10) && (base != 16))
        return 0;
    unsigned int size;
    int cnt, minus = 0;
    char s[21];
    const char* byte_to_char = "0123456789ABCDEF";

    if (base == 16)
        size = sizeof(t) * 2 + 1;
    else
    {
        size = 21;
        if (t < 0) minus = 1;
    }
    cnt = size - 1;
    s[cnt] = 0;
    while (cnt > 0)
    {
        --cnt;
        if (base == 10)
        {
            int d = t % 10;
            if (d < 0) d = -d;
            s[cnt] = byte_to_char[d];
            t /= 10;
        }
        else
        {
            s[cnt] = byte_to_char[t & 0x0f];
            t = t >> 4;
        }
        if (t == 0) break;
    }
    if (base == 10)
    {
        if (cnt <= 0) return 0;
        if (minus) s[--cnt] = '-';
    }

    if (sizeBuf >= (size - cnt))
        memcpy(buf, s + cnt, (size - cnt));
    else
        return 0;
    return size - cnt - 1;
}
//======================================================================
template <typename T>
std::string int_to_str(T t, int base)
{
    if ((base != 10) && (base != 16))
        return "";
    unsigned int size;
    int cnt, minus = 0;
    char s[21];
    const char* byte_to_char = "0123456789ABCDEF";

    if (base == 16)
        size = sizeof(t) * 2 + 1;
    else
    {
        size = 21;
        if (t < 0) minus = 1;
    }
    cnt = size - 1;
    s[cnt] = 0;
    while (cnt > 0)
    {
        --cnt;
        if (base == 10)
        {
            int d = t % 10;
            if (d < 0) d = -d;
            s[cnt] = byte_to_char[d];
            t /= 10;
        }
        else
        {
            s[cnt] = byte_to_char[t & 0x0f];
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
}
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

    template <typename T>
    void append_int(T t)
    {
        if (err) return;
        const unsigned long sz = 21;
        char s[sz];
        err = !int_to_str(t, s, sizeof(s), base);
        if (err == 0) append(s);
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
        lenBuf = b.lenBuf;
        sizeBuf = b.sizeBuf;

        b.ptr = NULL;
        b.sizeBuf = b.lenBuf = 0;
    }

    ~String() { if (ptr) delete[] ptr; }
    //------------------------------------------------
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
    }
    //----------------------- = ----------------------------
    String& operator = (const String & b)
    {
        if (err) return *this;
        if (this != &b)
        {
            lenBuf = 0;
            append(b);
        }
        return *this;
    }

    String& operator = (const char* s)
    {
        if (err) return *this;
        lenBuf = 0;
        append(s);
        return *this;
    }

    String& operator = (const std::string & s)
    {
        if (err) return *this;
        lenBuf = 0;
        append(s);
        return *this;
    }
    //----------------- << -----------------
    String& operator << (const String& b) { if (err) return *this; append(b); return *this; }
    String& operator << (const char ch) { if (err) return *this; append(ch); return *this; }
    String& operator << (const char* s) { if (err) return *this; append(s); return *this; }
    String& operator << (const std::string& s) { if (err) return *this; append(s); return *this; }

    String& operator << (const long long ll)
    {
        if (err) return *this;
        append_int(ll);
        return *this;
    }

    String& operator << (const long int li)
    {
        if (err) return *this;
        append_int(li);
        return *this;
    }

    String& operator << (const int i)
    {
        if (err) return *this;
        append_int(i);
        return *this;
    }

    String& operator << (const short sh)
    {
        if (err) return *this;
        append_int(sh);
        return *this;
    }

    String& operator << (const unsigned long int ul)
    {
        if (err) return *this;
        append_int(ul);
        return *this;
    }

    String& operator << (const unsigned int ui)
    {
        if (err) return *this;
        append_int(ui);
        return *this;
    }

    String& operator << (const unsigned short ush)
    {
        if (err) return *this;
        append_int(ush);
        return *this;
    }
    //--------------------------------------
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

    int error() { return err; }
    unsigned long len() { if (err) return 0; return lenBuf; }
    unsigned long size() { if (err) return 0; return sizeBuf; }
};

#endif