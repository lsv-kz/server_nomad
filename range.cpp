#include "main.h"

using namespace std;

//======================================================================
int Ranges::check_range()
{
    int numPart, maxIndex, n;
    Range* r = t;
    numPart = lenBuf;
    maxIndex = n = numPart - 1;

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

                if ((maxIndex > n) && (r[maxIndex].part_len > 0))
                {
                    r[n].start = r[maxIndex].start;
                    r[n].end = r[maxIndex].end;
                    r[n].part_len = r[maxIndex].part_len;
                    r[maxIndex].part_len = 0;
                    maxIndex--;
                    n = maxIndex - 1;
                }
                else
                {
                    maxIndex--;
                    n = maxIndex;
                }
                numPart--;
                i = n - 1;
            }
            else
                i--;
        }
        n--;
    }

    return numPart;
}
//======================================================================
int Ranges::check_str_range(char* sRange, int sizeStr)
{
    char* p0 = sRange, * p;
    stringstream ss;
    long long size = sizeFile;
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
                if ((ch >= '0') && (ch <= '9'))// [10-50]
                {
                    end = strtoll(p, &p, 10);
                    if ((*p != ',') && (*p != 0))
                        break;
                }
                else if ((ch == ',') || (ch == 0))// [10-]
                    end = size - 1;
                else
                    break;
            }
            else
            {
                err = "112";
                return 0;
            }
        }
        else if (ch == '-')
        {
            if ((*(p + 1) >= '0') && (*(p + 1) <= '9'))// [-50]
            {
                end = strtoll(p, &p, 10);
                if ((*p != ',') && (*p != 0))
                    break;
                start = size + end;
                end = size - 1;
            }
            else
            {
                err = "128";
                return 0;
            }
        }
        else
        {
            err = "134";
            break;
        }

        if (end >= size)
            end = size - 1;

        if ((start < size) && (end >= start) && (start >= 0))
        {
            ss << start << '-' << end;
            if (*p == ',')
                ss << ',';
            numPart++;
            if (*p == 0)
                break;
        }
        p++;
    }

    int len = ss.str().size();
    if (len < sizeStr)
        memcpy(sRange, ss.str().c_str(), len + 1);
    else
    {
        err = "158";
        numPart = 0;
    }

    return numPart;
}
//======================================================================
int Ranges::parse_ranges(char* s, int sizeStr, long long sz)
{
    sizeFile = sz;
    numPart = check_str_range(s, sizeStr);
    if (numPart > 0)
    {
        if (resize(numPart))
        {
            numPart = 0;
            return 0;
        }

        char* p = s;
        for (int i = 0; i < numPart; ++i)
        {
            long long start, end;
            start = strtoll(p, &p, 10);
            p++;
            end = strtoll(p, &p, 10);
            p++;
            (*this)({ start, end, end - start + 1 });
        }

        numPart = check_range();
    }
    return numPart;
}
