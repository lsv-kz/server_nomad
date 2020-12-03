#ifndef CLASS_STRING_H_
#define CLASS_STRING_H_
#include <iostream>
#include <cstring>

//----------------------------------------------------------------------
class String
{
protected:
    const int ADDITION = 32;
    int add = ADDITION;
    char* buf = NULL;
    unsigned long lenBuf = 0;
    unsigned long sizeBuf = 0;

    void append(const String& s)
    {
        if (s.lenBuf == 0) return;
        if ((lenBuf + s.lenBuf) >= sizeBuf)
        {
            if (resize(lenBuf + s.lenBuf + 1 + add)) throw ENOMEM;
            add += sizeBuf;
        }
        if (buf)
        {
            memcpy(buf + lenBuf, s.buf, s.lenBuf);
            lenBuf += s.lenBuf;
            *(buf + lenBuf) = 0;
        }
    }

    void append(const char* s)
    {
        if (!s) return;
        unsigned long len = strlen(s);
        if ((lenBuf + len) >= sizeBuf)
        {
            if (resize(lenBuf + len + 1 + add)) throw ENOMEM;
            add += sizeBuf;
        }
        if (buf)
        {
            memcpy(buf + lenBuf, s, len);
            lenBuf += len;
            *(buf + lenBuf) = 0;
        }
    }

    void append(const std::string & s)
    {
        if (s.size() == 0) return;
        unsigned long len = s.size();
        if ((lenBuf + len) >= sizeBuf)
        {
            if (resize(lenBuf + len + 1 + add)) throw ENOMEM;
            add += sizeBuf;
        }
        if (buf)
        {
            memcpy(buf + lenBuf, s.c_str(), len);
            lenBuf += len;
            *(buf + lenBuf) = 0;
        }
    }

public:
    String()
    {
        sizeBuf = lenBuf = 0;
        buf = NULL;
    }

    String(const char* s)
    {
        sizeBuf = lenBuf = 0;
        buf = NULL;
        append(s);
    }

    String(const std::string & s)
    {
        sizeBuf = lenBuf = 0;
        buf = NULL;
        append(s);
    }

    String(long long ll)
    {
        sizeBuf = lenBuf = 0;
        buf = NULL;
        const unsigned long sz = 21;
        char s[sz];
        snprintf(s, sz, "%lld", ll);
        append(s);
    }

    String(int n, const char ch)
    {
        sizeBuf = lenBuf = 0;
        buf = NULL;
        if (n == 0) return;
        if (resize(n)) return;
        add += sizeBuf;
    }

    String(const String & b)
    {
        sizeBuf = lenBuf = 0;
        buf = NULL;
        append(b);
    }

    String(String && b)
    {
        buf = b.buf;
        lenBuf = b.lenBuf;
        sizeBuf = b.sizeBuf;

        b.buf = NULL;
        b.sizeBuf = b.lenBuf = 0;
    }

    ~String()
    {
        if (buf)
            delete[] buf;
    }
    //------------------------------------------------
    int resize(unsigned int n)
    {
        if (n <= sizeBuf) return 1;
        char* newBuf;
        newBuf = new(std::nothrow) char[n];
        if (!newBuf)
            return 1;
        if (buf)
        {
            memcpy(newBuf, buf, lenBuf);
            delete[] buf;
        }
        *(newBuf + lenBuf) = '\0';
        sizeBuf = n;
        buf = newBuf;
        return 0;
    }
    //----------------------- = ----------------------------
    String& operator = (const String & b)
    {
        if (this != &b)
        {
            if (buf) buf[lenBuf = 0] = '\0';
            append(b);
        }
        return *this;
    }

    String& operator = (const char* s)
    {
        if (buf) buf[lenBuf = 0] = '\0';
        append(s);
        return *this;
    }

    String& operator = (const std::string & s)
    {
        if (buf) buf[lenBuf = 0] = '\0';
        append(s);
        return *this;
    }
    //------------------- + -----------------
    String& operator + (const String & b)
    {
        append(b);
        return *this;
    }

    String& operator + (const char* s)
    {
        append(s);
        return *this;
    }

    String& operator + (const std::string & s)
    {
        append(s);
        return *this;
    }

    String& operator + (const long long ll)
    {
        const unsigned long sz = 21;
        char s[sz];
        snprintf(s, sz, "%lld", ll);
        append(s);
        return *this;
    }
    //--------------------- += ----------------------
    String& operator += (const String & b)
    {
        append(b);
        return *this;
    }

    String& operator += (const char* s)
    {
        append(s);
        return *this;
    }

    String& operator += (const std::string & s)
    {
        append(s);
        return *this;
    }

    String& operator += (const long long ll)
    {
        const unsigned long sz = 21;
        char s[sz];
        snprintf(s, sz, "%lld", ll);
        append(s);
        return *this;
    }
    //----------------- << -----------------
    String& operator << (const String & b)
    {
        append(b);
        return *this;
    }

    String& operator << (const char* s)
    {
        append(s);
        return *this;
    }
    String& operator << (const std::string & s)
    {
        append(s);
        return *this;
    }

    String& operator << (const long long ll)
    {
        const unsigned long sz = 21;
        char s[sz];
        snprintf(s, sz, "%lld", ll);
        append(s);
        return *this;
    }
    //--------------------------------------
    String& operator () (const char* s1, const char* s2)
    {
        unsigned long len1 = strlen(s1), len2 = strlen(s2);
        if ((lenBuf + len1 + len2) >= sizeBuf)
            resize(lenBuf + len1 + len2 + 1);
        if (buf)
        {
            memcpy(buf + lenBuf, s1, len1);
            lenBuf += len1;
            memcpy(buf + lenBuf, s2, len2);
            lenBuf += len2;
            *(buf + lenBuf) = 0;
        }
        return *this;
    }

    const char* ptr() { return (buf ? buf : ""); }
    unsigned long len() { return lenBuf; }
    unsigned long size() { return sizeBuf; }
};

#endif