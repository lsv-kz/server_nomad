#ifndef CLASS_STRING_H_
#define CLASS_STRING_H_
#include <iostream>
#include <cstring>

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
            resize(lenBuf + s.lenBuf + 1 + add);
            if (err) return;
        }
        if (ptr)
        {
            memcpy(ptr + lenBuf, s.ptr, s.lenBuf);
            lenBuf += s.lenBuf;
        }
    }

    void append(const char ch)
    {
        if (err) return;
        unsigned long len = 1;
        if ((lenBuf + len) >= sizeBuf)
        {
            resize(lenBuf + len + 1 + add);
            if (err) return;
        }
        if (ptr)
        {
            memcpy(ptr + lenBuf, &ch, len);
            lenBuf += len;
        }
    }

    void append(const char* s)
    {
        if (!s || err) return;
        unsigned long len = strlen(s);
        if ((lenBuf + len) >= sizeBuf)
        {
            resize(lenBuf + len + 1 + add);
            if (err) return;
        }
        if (ptr)
        {
            memcpy(ptr + lenBuf, s, len);
            lenBuf += len;
        }
    }

    void append(const std::string & s)
    {
        if ((s.size() == 0) || err) return;
        unsigned long len = s.size();
        if ((lenBuf + len) >= sizeBuf)
        {
            resize(lenBuf + len + 1 + add);
            if (err) return;
        }
        if (ptr)
        {
            memcpy(ptr + lenBuf, s.c_str(), len);
            lenBuf += len;
        }
    }

public:
    String() { sizeBuf = lenBuf = err = 0; ptr = NULL; }

    String(const char ch)
    {
        sizeBuf = lenBuf = err = 0;
        ptr = NULL;
        append(ch);
    }
    
    String(const char* s)
    {
        sizeBuf = lenBuf = err = 0;
        ptr = NULL;
        append(s);
    }

    String(const std::string & s)
    {
        sizeBuf = lenBuf = err = 0;
        ptr = NULL;
        append(s);
    }

    String(long long ll)
    {
        sizeBuf = lenBuf = err = 0;
        ptr = NULL;
        const unsigned long sz = 21;
        char s[sz];
        if (base == 16)
            snprintf(s, sz, "%llX", ll);
        else
            snprintf(s, sz, "%lld", ll);
        append(s);
    }

    String(int n, const char ch)
    {
        sizeBuf = lenBuf = err = 0;
        ptr = NULL;
        if (n == 0) return;
        resize(n);
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

    ~String()
    {
        if (ptr) delete[] ptr;
    }
    //------------------------------------------------
    void resize(unsigned int n)
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

    String& operator = (const char ch)
    {
        if (err) return *this;
        lenBuf = 0;
        append(ch);
        return *this;
    }

    String& operator = (const std::string & s)
    {
        if (err) return *this;
        lenBuf = 0;
        append(s);
        return *this;
    }
    //------------------- + -----------------
    String& operator + (const String& S) { if (err) return *this; append(S); return *this; }
    String& operator + (const char* s) { if (err) return *this; append(s); return *this; }
    String& operator + (const char ch) { if (err) return *this; append(ch); return *this; }
    String& operator + (const std::string& s) { if (err) return *this; append(s); return *this; }

    String& operator + (const long long ll)
    {
        if (err) return *this;
        const unsigned long sz = 21;
        char s[sz];
        if (base == 16)
            snprintf(s, sz, "%llX", ll);
        else
            snprintf(s, sz, "%lld", ll);
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
        const unsigned long sz = 21;
        char s[sz];
        if (base == 16)
            snprintf(s, sz, "%llX", ll);
        else
            snprintf(s, sz, "%lld", ll);
        append(s);
        return *this;
    }

    String& operator << (const long int li)
    {
        if (err) return *this;
        const unsigned long sz = 16;
        char s[sz];
        if (base == 16)
            snprintf(s, sz, "%lX", li);
        else
            snprintf(s, sz, "%ld", li);
        append(s);
        return *this;
    }

    String& operator << (const int i)
    {
        if (err) return *this;
        const unsigned long sz = 16;
        char s[sz];
        if (base == 16)
            snprintf(s, sz, "%X", i);
        else
            snprintf(s, sz, "%i", i);
        append(s);
        return *this;
    }

    String& operator << (const unsigned long int i)
    {
        if (err) return *this;
        const unsigned long sz = 16;
        char s[sz];
        if (base == 16)
            snprintf(s, sz, "%lX", i);
        else
            snprintf(s, sz, "%lu", i);
        append(s);
        return *this;
    }

    String& operator << (const unsigned int i)
    {
        if (err) return *this;
        const unsigned long sz = 16;
        char s[sz];
        if (base == 16)
            snprintf(s, sz, "%X", i);
        else
            snprintf(s, sz, "%u", i);
        append(s);
        return *this;
    }
    //--------------------------------------
    String& operator << (BaseString b)
    {
        if (err) return *this;
        base = b.b;
        return *this;
    }

    int error() { return err; }

    const char* str()
    {
        if (err || (!ptr)) return "";
        *(ptr + lenBuf) = 0;
        return ptr;
    }

    unsigned long len() { if (err) return 0; return lenBuf; }
    unsigned long size() { if (err) return 0; return sizeBuf; }
};

#endif