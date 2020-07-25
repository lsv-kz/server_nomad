#include "main.h"

using namespace std;

//======================================================================
int check_range(Connect* req)
{
    int size, numPart, m, n;
    struct Range* r = req->resp.rangeBytes;
    size = numPart = req->resp.numPart;
    m = n = size - 1;

    while (n > 0)
    {
        for (int i = n - 1; i >= 0; )
        {
            if (((r[n].end + 1) >= r[i].start) && ((r[i].end + 1) >= r[n].start))
            {
                if (r[n].start < r[i].start)
                    r[i].start = r[n].start;

                if (r[n].end > r[i].end)
                    r[i].end = r[n].end;

                r[i].part_len = r[i].end - r[i].start + 1;
                r[n].part_len = 0;

                if ((m > n) && (r[m].part_len > 0))
                {
                    r[n].start = r[m].start;
                    r[n].end = r[m].end;
                    r[n].part_len = r[m].part_len;
                    r[m].part_len = 0;
                    m--;
                    n = m - 1;
                }
                else
                {
                    m--;
                    n = m;
                }
                numPart--;
                i = n - 1;
            }
            else
                i--;
        }
        n--;
    }

    req->resp.numPart = numPart;
    return numPart;
}
//======================================================================
int check_str_range(Connect* req)
{
    char* p0 = req->sRange, * p;
    stringstream ss;
    long long size = req->resp.fileSize;
    int numPart = 0;

    for (; *p0; p0++)
    {
        if ((*p0 != ' ') && (*p0 != '\t'))
            break;
    }

    for (p = p0; *p; )
    {
        long long start = 0, end = 0;
        char ch = *p;
        if ((ch >= '0') && (ch <= '9'))
        {
            start = strtoll(p, &p, 10);
            if (*p == '-')
            {
                ch = *(++p);
                if ((ch >= '0') && (ch <= '9'))// [start-end]
                {
                    end = strtoll(p, &p, 10);
                    if ((*p != ',') && (*p != ' ') && (*p != 0))
                        break;
                }
                else if ((ch == ',') || (ch == 0))// [start-]
                    end = size - 1;
                else
                    break;
            }
            else
                start = -1;
        }
        else if (ch == '-')
        {
            if ((*(p + 1) >= '0') && (*(p + 1) <= '9'))// [-end]
            {
                end = strtoll(p, &p, 10);
                if ((*p != ',') && (*p != 0))
                    break;
                start = size + end;
                end = size - 1;
            }
            else
                break;
        }
        else
            break;

        if (end >= size)
            end = size - 1;

        if ((start < size) && (end >= start) && (start >= 0))
        {
            ss << start << '-';
            ss << end;
            if (*p == ',')
                ss << ',';
            numPart++;
            if (*p == 0)
                break;
        }
        p++;
    }

    int len = (int)ss.str().size();
    if (len < (int)sizeof(req->sRange))
        memcpy(req->sRange, ss.str().c_str(), len + (int)1);
    else
        numPart = 0;

    req->resp.numPart = numPart;
    return numPart;
}
//======================================================================
int parse_range(Connect* req)
{
    int n = check_str_range(req);
    if (n <= 0)
        return n;

    req->resp.rangeBytes = new(nothrow) Range[req->resp.numPart];
    if (!req->resp.rangeBytes)
        return req->resp.numPart;

    char* p = req->sRange;
    for (int i = 0; i < req->resp.numPart; ++i)
    {
        long long start, end;
        start = strtoll(p, &p, 10);
        p++;
        end = strtoll(p, &p, 10);
        p++;
        req->resp.rangeBytes[i] = { start, end, end - start + 1 };
    }

    if (req->resp.numPart > 1)
    {
        return check_range(req);
    }
    else if (req->resp.numPart == 1)
    {
        req->resp.offset = req->resp.rangeBytes[0].start;
        req->resp.respContentLength = req->resp.rangeBytes[0].part_len;
    }
    else
    {
        print_err("%d<%s:%d> Error parse_range()=%d\n", req->numChld, __func__, __LINE__, req->resp.numPart);
    }

    return req->resp.numPart;
}
