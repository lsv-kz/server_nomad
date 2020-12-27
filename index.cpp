#include "classes.h"

using namespace std;

struct stFile
{
    string name;
    long long size;
};
int index_chunk(Connect* req, vector <string>& vecDirs, vector <struct stFile>& vecFiles);
//======================================================================
static int isimage(const char* name)
{
    const char* p;

    p = strrchr(name, '.');
    if (!p)
        return 0;

    if (!strlcmp_case(p, (char*)".gif", 4)) return 1;
    else if (!strlcmp_case(p, (char*)".png", 4)) return 1;
    else if (!strlcmp_case(p, (char*)".ico", 4)) return 1;
    else if (!strlcmp_case(p, (char*)".svg", 4)) return 1;
    else if (!strlcmp_case(p, (char*)".jpeg", 5) || !strlcmp_case(p, (char*)".jpg", 4)) return 1;
    return 0;
}
//======================================================================
static int isaudiofile(const char* name)
{
    const char* p;

    if (!(p = strrchr(name, '.'))) return 0;

    if (!strlcmp_case(p, (char*)".wav", 4)) return 1;
    else if (!strlcmp_case(p, (char*)".mp3", 4)) return 1;
    else if (!strlcmp_case(p, (char*)".ogg", 4)) return 1;
    return 0;
}
//======================================================================
bool compareVec(stFile s1, stFile s2)
{
    return (s1.name < s2.name);
}
//======================================================================
int index_dir(RequestManager * ReqMan, Connect* req, wstring & path)
{    
    int dirs, files;
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = INVALID_HANDLE_VALUE;

    vector <string> vecDirs;
    vector <struct stFile> vecFiles;

    path += L"/*";
    hFind = FindFirstFileW(path.c_str(), &ffd);
    if (INVALID_HANDLE_VALUE == hFind)
    {
        string str;
        utf16_to_utf8(str, path);
        print_err(req, "<%s:%d>  Error opendir(\"%s\")\n", __func__, __LINE__, str.c_str());
        return -RS500;
    }

    dirs = files = 0;
    do
    {
        if (ffd.cFileName[0] == '.') continue;

        string fname;
        int err = utf16_to_utf8(fname, ffd.cFileName);
        if (err == 0)
        {
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (dirs > 255) continue;
                vecDirs.push_back(fname);
                dirs++;
            }
            else
            {
                if (files > 255) continue;
                stFile tmp;
                tmp.name = fname; // 
                tmp.size = ((ffd.nFileSizeHigh * (MAXDWORD + 1LL)) + ffd.nFileSizeLow);
                vecFiles.push_back(tmp);
                files++;
            }
        }
    } while (FindNextFileW(hFind, &ffd) != 0);
    FindClose(hFind);

    sort(vecDirs.begin(), vecDirs.end());
    sort(vecFiles.begin(), vecFiles.end(), compareVec);

    return index_chunk(req, vecDirs, vecFiles);
}
//======================================================================
int index_chunk(Connect* req, vector <string> & vecDirs, vector <struct stFile> & vecFiles)
{
    int n;
    int dirs = vecDirs.size(), files = vecFiles.size();
    int chunk;
    if (req->reqMethod == M_HEAD)
        chunk = NO_SEND;
    else
        chunk = ((req->httpProt == HTTP11) && req->connKeepAlive) ? SEND_CHUNK : SEND_NO_CHUNK;
    
    ClChunked chunk_buf(req->clientSocket, chunk);

    req->resp.respStatus = RS200;
    String hdrs(80);
    if (hdrs.error())
    {
        print_err(req, "<%s:%d> Error create String object\n", __func__, __LINE__);
        return -RS500;
    }

    if (chunk == SEND_CHUNK)
    {
        hdrs << "Transfer-Encoding: chunked\r\n";      
    }

    hdrs << "Content-Type: text/html; charset=utf-8\r\n";
    req->resp.respContentLength = -1;

    if (chunk)
    {
        if (send_response_headers(req, &hdrs))
        {
            return -1;
        }
    }

    chunk_buf << "<!DOCTYPE HTML>\n"
            "<html>\n"
            " <head>\n"
            "  <meta charset=\"UTF-8\">\n"
            "  <title>Index of " << req->decodeUri << "</title>\n"
            "  <style>\n"
            "    body {\n"
            "     margin-left:100px; margin-right:50px;\n"
            "    }\n"
            "  </style>\n"
            "  <link href=\"styles.css\" type=\"text/css\" rel=\"stylesheet\">"
            " </head>\n"
            " <body id=\"top\">\n"
            "  <h3>Index of " << req->decodeUri << "</h3>\n"
            "  <table cols=\"2\" width=\"100\x25\">\n"
            "   <tr><td><h3>Directories</h3></td><td></td></tr>\n";
    if (chunk_buf.error())
    {
        print_err(req, "<%s:%d>   Error chunk\n", __func__, __LINE__);
        return -1;
    }
    //------------------------------------------------------------------
   
    if (!strcmp(req->decodeUri, "/"))
            chunk_buf << "   <tr><td></td><td></td></tr>\n";
    else
            chunk_buf << "   <tr><td><a href=\"../\">Parent Directory/</a></td><td></td></tr>\n";
    if (chunk_buf.error())
    {
        print_err(req, "<%s:%d>   Error chunk\n", __func__, __LINE__);
        return -1;
    }
    //-------------------------- Directories ---------------------------
    for (int i = 0; i < dirs; ++i)
    {
        chunk_buf << "   <tr><td><a href=\"" << vecDirs[i] << "/\">" << vecDirs[i] << "/</a></td>"
                "<td align=right></td></tr>\n";
        if (chunk_buf.error())
        {
            print_err(req, "<%s:%d>   Error chunk\n", __func__, __LINE__);
            return -1;
        }
    }
    //------------------------------------------------------------------
    chunk_buf << "   <tr><td><hr></td><td><hr></td></tr>\n"
            "   <tr><td><h3>Files</h3></td><td></td></tr>\n";
    if (chunk_buf.error())
    {
        print_err(req, "<%s:%d>   Error chunk\n", __func__, __LINE__);
        return -1;
    }
    //---------------------------- Files -------------------------------
    for (int i = 0; i < files; ++i)
    {
            if (isimage(vecFiles[i].name.c_str()) && (conf->ShowMediaFiles == 'y'))
            {
                if (vecFiles[i].size < 20000)
                    chunk_buf << "   <tr><td><a href=\"" << vecFiles[i].name << "\"><img src=\"" <<
                    vecFiles[i].name << "\"></a><br>";
                else
                    chunk_buf << "   <tr><td><a href=\"" << vecFiles[i].name << "\"><img src=\"" << vecFiles[i].name <<
                    "\" width=\"320\"></a><br>";
                chunk_buf << vecFiles[i].name << "</td><td align=\"right\">" << vecFiles[i].size << " bytes</td></tr>\n"
                    "   <tr><td></td><td></td></tr>\n";
            }
            else if (isaudiofile(vecFiles[i].name.c_str()) && (conf->ShowMediaFiles == 'y'))
                chunk_buf << "   <tr><td><audio preload=\"none\" controls src=\"" << vecFiles[i].name << "\"></audio>"
                "<a href=\"" << vecFiles[i].name << "\">" << vecFiles[i].name <<
                "</a></td><td align=\"right\">" << vecFiles[i].size << " bytes</td></tr>\n";
            else
                chunk_buf << "   <tr><td><a href=\"" << vecFiles[i].name << "\">" << vecFiles[i].name << "</a></td>"
                "<td align=\"right\">" << vecFiles[i].size << " bytes</td></tr>\n";
        if (chunk_buf.error())
        {
            print_err(req, "<%s:%d>   Error chunk\n", __func__, __LINE__);
            return -1;
        }
    }
    //------------------------------------------------------------------
    chunk_buf << "  </table>\n"
            "  <hr>\n"
            "  " << req->resp.sLogTime <<
            "\n"
            "  <a href=\"#top\" style=\"display:block;\n"
            "         position:fixed;\n"
            "         bottom:30px;\n"
            "         left:10px;\n"
            "         width:50px;\n"
            "         height:40px;\n"
            "         font-size:60px;\n"
            "         background:gray;\n"
            "         border-radius:10px;\n"
            "         color:black;\n"
            "         opacity: 0.7\">^</a>\n"
            " </body>\n"
            "</html>";
    if (chunk_buf.error())
    {
        print_err(req, "<%s:%d>   Error chunk\n", __func__, __LINE__);
        return -1;
    }
    //------------------------------------------------------------------
    n = chunk_buf.end();
    req->resp.respContentLength = chunk_buf.all();
    if (n < 0)
    {
        print_err(req, "<%s:%d>   Error chunk_buf.end(): %d\n", __func__, __LINE__, n);
        return -1;
    }

    if (chunk == NO_SEND)
    {
        if (send_response_headers(req, &hdrs))
        {
            print_err("%d<%s:%d> Error send_response_headers()\n", req->numChld, __func__, __LINE__);
            return -1;
        }
    }
    else
        req->resp.send_bytes = req->resp.respContentLength;

    return 0;
}

