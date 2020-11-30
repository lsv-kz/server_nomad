#ifndef CLASSES_H_
#define CLASSES_H_
#include <iostream>
#include <cstring>


//----------------------------------------------------------------------
class String
{
protected:
    const int ADDITION = 32;
    char *buf = NULL;
    unsigned long lenBuf = 0;
    unsigned long sizeBuf = 0;
    
    void append(const String & s)
    {
        if (s.lenBuf == 0) return;
        if ((lenBuf + s.lenBuf) >= sizeBuf)
            if (resize(lenBuf + s.lenBuf + 1 + ADDITION)) return;
        if (buf)
        {
            memcpy(buf + lenBuf, s.buf, s.lenBuf);
            lenBuf += s.lenBuf;
            *(buf + lenBuf) = 0;
        }
    }
    
    void append(const char * s)
    {
        if (!s) return;
        unsigned long len = strlen(s);
        if ((lenBuf + len) >= sizeBuf)
            if (resize(lenBuf + len + 1 + ADDITION)) return;
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
            if (resize(lenBuf + len + 1 + ADDITION)) return;
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
    
    String(const char * s)
    {
        sizeBuf = lenBuf = 0;
        buf = NULL;
        append(s);
    }
    
    String(const std::string& s)
    {
        sizeBuf = lenBuf = 0;
        buf = NULL;
        append(s);
    }
    
    String(const String& b)
    {
        sizeBuf = lenBuf = 0;
        buf = NULL;
        append(b);
    }
    
    String(String&& b)
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
            delete [] buf;
    }
    
    int resize(unsigned int n)
    {
        if (n <= sizeBuf) return 1;
        char *newBuf;
        newBuf = new(std::nothrow) char [n];
        if (!newBuf)
            return 1;
        if (buf)
        {
            memcpy(newBuf, buf, lenBuf);
            delete [] buf;
        }
        *(newBuf + lenBuf) = '\0';
        sizeBuf = n;
        buf = newBuf;
        return 0;
    }
//----------------------- = ----------------------------
    String & operator = (const String& b)
    {
        if (this != &b)
        {
            if (buf) buf[lenBuf = 0] = '\0';
            append(b);
        }
        return *this;
    }

    String & operator = (const char *s)
    {
        if (buf) buf[lenBuf = 0] = '\0';
        append(s);
        return *this;
    }
    
    String & operator = (const std::string& s)
    {
        if (buf) buf[lenBuf = 0] = '\0';
        append(s);
        return *this;
    }
//------------------- const char * -----------------
    String & operator << (const char *s)
    {
        append(s);
        return *this;
    }
    
    String & operator + (const char *s)
    {
        append(s);
        return *this;
    }
    
    String & operator += (const char *s)
    {
        append(s);
        return *this;
    }
//--------------------- std::string& ----------------------
    String & operator << (const std::string& s)
    {
        append(s);
        return *this;
    }
    
    String & operator + (const std::string& s)
    {
        append(s);
        return *this;
    }
    
    String & operator += (const std::string& s)
    {
        append(s);
        return *this;
    }
//----------------- const Buff& -----------------
    String & operator << (const String& b)
    {
        append(b);
        return *this;
    }
    
    String & operator + (const String& b)
    {
        append(b);
        return *this;
    }
    
    String & operator += (const String& b)
    {
        append(b);
        return *this;
    }
//--------------------------------------
    String & operator () (const char *s1, const char *s2)
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

    String & operator << (const long long ll)
    {
        const unsigned long sz = 21;
        char s[sz];
        snprintf(s, sz, "%lld", ll);
        append(s);
        return *this;
    }
    
    void append(const char *s, unsigned long n)
    {
        unsigned int len = strlen(s);
        if (n > len) n = len;
        if ((lenBuf + n) >= sizeBuf)
            resize(lenBuf + n + 1);
        if (buf)
        {
            memcpy(buf + lenBuf, s, n);
            lenBuf += n;
            *(buf + lenBuf) = 0;
        }
    }

    const char *ptr() { return (buf ? buf : ""); }
    unsigned long len() { return lenBuf; }
    unsigned long size() { return sizeBuf; }
};
//======================================================================
template <typename T>
class Array
{
protected:
    const int ADDITION = 8;
    T *t;
    unsigned int sizeBuf;
    unsigned int lenBuf;
    const char *err = "Success";
    
    int append(const T& val)
    {
        if (lenBuf >= sizeBuf)
            if (resize(sizeBuf + ADDITION)) return 1;
        t[lenBuf++] = val;
        return 0;
    }
    
public:
    Array(const Array&) = delete;
    Array()
    {
        sizeBuf = lenBuf = 0;
        t = NULL;
    }
    
    Array(unsigned int n)
    {
        lenBuf = 0;
        t = new(std::nothrow) T [n];
        if (!t)
            sizeBuf = 0;
        else
            sizeBuf = n;
    }
    
    ~Array()
    {
        if (t)
        {
            delete [] t;
        }
    }
    
    Array<T> & operator << (const T& val)
    {
        append(val);
        return *this;
    }
    
    Array<T> & operator + (const T& val)
    {
        append(val);
        return *this;
    }

    int resize(unsigned int n)
    {
        if (n <= lenBuf)
            return 1;
        T *tmp = new(std::nothrow) T [n];
        if (!tmp)
            return 1;
        for (unsigned int c = 0; c < lenBuf; ++c)
            tmp[c] = t[c];
        if (t)
            delete [] t;
        t = tmp;
        sizeBuf = n;
        return 0;
    }
    
    const char *error()
    {
        const char *p = err;
        err = "Success";
        return p;
    }
    
    int operator () (const T& val)
    {
        return append(val);
    }
    
    int operator () (const T& val1, const T& val2)
    {
        if (lenBuf >= sizeBuf)
            if (resize(sizeBuf + ADDITION)) return 1;

        t[lenBuf] = val1;
        t[lenBuf++] << val2;
        return 0;
    }
    
    T *get(unsigned int i)
    {
        if (i < lenBuf)
            return t + i;
        else
            return NULL;
    }
    
    int len() { return lenBuf; }
    int size() { return sizeBuf; }
};
//----------------------------------------------------------------------
struct Range {
    long long start;
    long long end;
    long long part_len;
};

class Ranges : public Array <Range>
{
protected:
    int numPart;
    long long sizeFile;

public:

    int check_range();
    int check_str_range(char *sRange, int sizeStr);
    int parse_ranges(char *s, int sizeStr, long long sz);
};

#endif
