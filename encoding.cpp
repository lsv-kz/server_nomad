#include "main.h"

//======================================================================
int utf16_to_mbs(string& s, const wchar_t *ws)
{
	size_t wlen = wcslen(ws), ret = 0;
	size_t  size = wlen * sizeof(wchar_t);
	char *buf = new(nothrow) char [size + 1];
	if (!buf)
	{
		s = "Error new()";
		return 0;
	}

	size_t  n;
	errno_t err = wcstombs_s(&n, buf, size + 1, ws, size);
	if (!err)
	{
		if (n == (size-1)) buf[size-1] = 0;
		s = buf;
		ret = s.size();
	}
	else
	{
		char sTmp[256];
		if (strerror_s(sTmp, 256, errno))
			s[0] = 0;
		s = "Error wcstombs() ";
		s += sTmp;
		ret = 0;
	}
	delete [] buf;
	return (int)ret;
}
//======================================================================
int mbs_to_utf16(wstring& ws, const char *u8)
{
	size_t len = strlen(u8);
	wchar_t *wchs;
	wchs = new(nothrow) wchar_t [len + 1];
	if (!wchs)
	{
		ws = L"Error new()";
		return 0;
	}

	size_t  ret;
	errno_t err = mbstowcs_s(&ret, wchs, len + 1, u8, len);
	if (!err)
	{
		if (ret == len) wchs[len] = L'\0';
		ws = wchs;
		ret = ws.size();
	}
	else
	{
		ws = L"Error mbstowcs() ";
		ret = 0;
	}
	delete [] wchs;
	return (int)ret;
}
//======================================================================
int utf16_to_utf8(string& s, wstring& ws)
{
	size_t wlen = ws.size(), i = 0;
	s.clear();

	while (i < wlen)
	{
		wchar_t wc = ws[i];
		if (wc <= 0x7f)
		{
			s += (char)wc;
		}
		else if (wc <= 0x7ff)
		{
			s += (0xc0 | (char)(wc>>6));
			s += (0x80 | ((char)wc & 0x3f));
		}
		else if (wc <= 0xffff)
		{
			s += (0xE0 | (wc >> 12));
			s += (0x80 | ((wc >> 6) & 0x3F));
			s += (0x80 | ((wc >> 0) & 0x3F));
		}
		else
		{
			print_err("<%s:%d> utf-32\n", __func__, __LINE__);
			return 1;
		}
		++i;
	}

	return 0;
}
//======================================================================
int utf16_to_utf8(string& s, const wchar_t *ws)
{
	size_t wlen = wcslen(ws), i = 0; 
	s.clear();

	while (i < wlen)
	{
		wchar_t wc = ws[i];
		if (wc <= 0x7f)
		{
			s += (char)wc;
		}
		else if (wc <= 0x7ff)
		{
			s += (0xc0 | (char)(wc>>6));
			s += (0x80 | ((char)wc & 0x3f));
		}
		else if (wc <= 0xffff)
		{
			s += (0xE0 | (wc >> 12));
			s += (0x80 | ((wc >> 6) & 0x3F));
			s += (0x80 | ((wc >> 0) & 0x3F));
		}
		else
		{
			printf("<%s:%d> utf-32\n", __func__, __LINE__);
			return 1;
		}
		++i;
	}

	return 0;
}
//======================================================================
int utf8_to_utf16(char *u8, wstring& ws)
{
	size_t len = strlen(u8), i = 0;
	size_t num;
	ws.clear();

	while (i < len)
	{
		unsigned char ch = u8[i++];
		wchar_t uni;
		
		if (ch <= 0x7F)
        {
            uni = ch;
            num = 0;
        }
        else if (ch <= 0xBF)
        {
            ws = L"not a UTF-8 string\n";
            return 1;
        }
		else if (ch <= 0xDF)
        {
            uni = ch & 0x1F;
            num = 1;
        }
        else if (ch <= 0xEF)
        {
            uni = ch & 0x0F;
            num = 2;
        }
        else if (ch <= 0xF7)
        {
            uni = ch & 0x07;
            num = 3;
        }
        else
        {
            ws = L"not a UTF-8 string\n";
            return 2;
        }
        
        for (size_t j = 0; j < num; ++j)
        {
            if (i == len)
            {
                ws = L"not a UTF-8 string\n";
				return 3;
			}
			
            unsigned char ch = u8[i++];
            if (ch < 0x80 || ch > 0xBF)
            {
				ws = L"not a UTF-8 string\n";
				return 4;
			}
                
            uni <<= 6;
            uni += ch & 0x3F;
        }
        if (uni >= 0xD800 && uni <= 0xDFFF)
        {
			ws = L"not a UTF-8 string\n";
            return 5;
		}
        
        if (uni > 0x10FFFF)
        {
			ws = L"not a UTF-8 string\n";
            return 6;
		}

		ws += uni;
	}
	
	return 0;
}
//======================================================================
int utf8_to_utf16(string& u8, wstring& ws)
{
	size_t len = u8.size(), i = 0;
	size_t num;
	ws.clear();
	
	while (i < len)
	{
		unsigned char ch = u8[i++];
		wchar_t uni;
		
		if (ch <= 0x7F)
        {
            uni = ch;
            num = 0;
        }
        else if (ch <= 0xBF)
        {
            ws = L"not a UTF-8 string\n";
            return 1;
        }
		else if (ch <= 0xDF)
        {
            uni = ch & 0x1F;
            num = 1;
        }
        else if (ch <= 0xEF)
        {
            uni = ch & 0x0F;
            num = 2;
        }
        else if (ch <= 0xF7)
        {
            uni = ch & 0x07;
            num = 3;
        }
        else
        {
            ws = L"not a UTF-8 string\n";
            return 2;
        }
        
        for (size_t j = 0; j < num; ++j)
        {
            if (i == len)
            {
                ws = L"not a UTF-8 string\n";
				return 3;
			}
			
            unsigned char ch = u8[i++];
            if (ch < 0x80 || ch > 0xBF)
            {
				ws = L"not a UTF-8 string\n";
				return 4;
			}
                
            uni <<= 6;
            uni += ch & 0x3F;
        }
        
        if (uni >= 0xD800 && uni <= 0xDFFF)
        {
			ws = L"not a UTF-8 string\n";
            return 5;
		}
        
        if (uni > 0x10FFFF)
        {
			ws = L"not a UTF-8 string\n";
            return 6;
		}

		ws += uni;
	}
	
	return 0;
}
/*====================================================================*/
int decode(char *s_in, size_t len_in, char *s_out, int len)
{
	char tmp[3];
	char *p = s_out;
	char hex[] = "ABCDEFabcdef0123456789";
	unsigned char c;

    int cnt = 0, i;

	while(len >= 1)
    {
		c = *(s_in++);
		
		len--;
		cnt++;
		if(c == '%')
		{
			tmp[0] = *(s_in++);
			--len_in;
			if (!len_in) break;
			tmp[1] = *(s_in++);
			--len_in;
			if (!len_in) break;
			tmp[2] = 0;
			if(strspn(tmp, hex) != 2)
			{
				if(s_out)
					*p = 0;
				return 0;
			}
			sscanf_s(tmp, "%x", &i);
			if(s_out)
				*p++ = (char)i;
			len -= 2;
		}
	/*	else if(c == '+')
		{
			if(s_out)
				*p++ = ' ';
		}*/
		else
		{
			if(s_out)
				*p++ = c;
		}
		
		--len_in;
		if (!len_in) break;
    }
    if(s_out)
		*p = 0;
    return cnt;
}
