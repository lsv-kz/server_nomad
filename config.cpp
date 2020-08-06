#include "main.h"

using namespace std;

static Config c;
const Config* const conf = &c;
//======================================================================
int check_path(wstring& path)
{
    wchar_t cwd[2048], cwd2[2048];

    if (!(_wgetcwd(cwd, sizeof(cwd) / sizeof(wchar_t))))
        return -1;

    if (_wchdir(path.c_str()))
        return -1;

    if (!(_wgetcwd(cwd2, sizeof(cwd2) / sizeof(wchar_t))))
        return -1;

    if (cwd2[wcslen(cwd2) - 1] == '/')
        cwd2[wcslen(cwd2) - 1] = 0;
    path = cwd2;

    if (_wchdir(cwd))
        return -1;

    return 0;
}
//======================================================================
int read_conf_file(const char* path_conf)
{
    stringstream ss;
    string s, nameFile;
    nameFile += path_conf;
    nameFile += "/server.conf";

    ifstream fconf(nameFile.c_str(), ios::binary);
    if (!fconf.is_open())
    {
        cerr << __func__ << "(): Error create conf file (" << nameFile << ")\n";
        cin.get();
        exit(1);
    }

    while (!fconf.eof())
    {
        ss.clear();
        ss.str("");
        getline(fconf, s);
        ss << s;
        ss >> s;
        if (s[0] == '#')
            continue;

        if (s == "ServerAddr")
            ss >> c.host;
        else if (s == "Port")
            ss >> c.servPort;
        else if (s == "ServerSoftware")
            ss >> c.ServerSoftware;
        else if (s == "SockBufSize")
            ss >> c.SOCK_BUFSIZE;
        else if (s == "DocumentRoot")
        {
            string tmp;
            ss >> tmp;
            utf8_to_utf16(tmp, c.wRootDir);
        }
        else if (s == "ScriptPath")
        {
            string stmp;
            ss >> stmp;
            mbs_to_utf16(c.wCgiDir, stmp.c_str());
        }
        else if (s == "LogPath")
        {
            string stmp;
            ss >> stmp;
            mbs_to_utf16(c.wLogDir, stmp.c_str());
        }
        else if (s == "ListenBacklog")
            ss >> c.ListenBacklog;
        else if (s == "MaxRequests")
            ss >> c.MaxRequests;
        else if (s == "SizeQueue")
            ss >> c.SizeQueue;
        else if (s == "NumChld")
            ss >> c.NumChld;
        else if (s == "MaxThreads")
            ss >> c.MaxThreads;
        else if (s == "MinThreads")
            ss >> c.MinThreads;
        else if (s == "KeepAlive")
            ss >> c.KeepAlive;
        else if (s == "TimeoutKeepAlive")
            ss >> c.TimeoutKeepAlive;
        else if (s == "TimeOut")
            ss >> c.TimeOut;
        else if (s == "TimeOutCGI")
            ss >> c.TimeOutCGI;
        else if (s == "TimeoutThreadCond")
            ss >> c.TimeoutThreadCond;
        else if (s == "PerlPath")
        {
            string stmp;
            ss >> stmp;
            mbs_to_utf16(c.wPerlPath, stmp.c_str());
        }
        else if (s == "PyPath")
        {
            string stmp;
            ss >> stmp;
            mbs_to_utf16(c.wPyPath, stmp.c_str());
        }
        else if (s == "PathPHP")
        {
            string stmp;
            ss >> stmp;
            mbs_to_utf16(c.wPathPHP, stmp.c_str());
        }
        else if (s == "UsePHP")
            ss >> c.usePHP;
        else if (s == "ShowMediaFiles")
            ss >> c.ShowMediaFiles;
        else if (s == "ClientMaxBodySize")
            ss >> c.ClientMaxBodySize;
        else if (s == "index.html")
            ss >> c.index_html;
        else if (s == "index.php")
            ss >> c.index_php;
        else if (s == "index.pl")
            ss >> c.index_pl;
    }

    fconf.close();

    //-------------------------log_dir--------------------------------------
    if (check_path(c.wLogDir) == -1)
    {
        wcerr << L" Error log_dir: " << c.wLogDir << L"\n";
        cin.get();
        exit(1);
    }
    path_correct(c.wLogDir);
    //------------------------------------------------------------------
    if (check_path(c.wRootDir) == -1)
    {
        wcerr << L"!!! Error root_dir: " << c.wRootDir << L"\n";
        cin.get();
        exit(1);
    }

    path_correct(c.wRootDir);
    if (c.wRootDir[c.wRootDir.size() - 1] == L'/')
        c.wRootDir.resize(c.wRootDir.size() - 1);
    //------------------------------------------------------------------
    if (check_path(c.wCgiDir) == -1)
    {
        wcerr << L"!!! Error cgi_dir: " << c.wCgiDir << L"\n";
        cin.get();
        exit(1);
    }
    path_correct(c.wCgiDir);
    if (c.wCgiDir[c.wCgiDir.size() - 1] == L'/')
        c.wCgiDir.resize(c.wCgiDir.size() - 1);

    return 0;
}
