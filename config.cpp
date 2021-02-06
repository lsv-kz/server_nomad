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
fcgi_list_addr* create_fcgi_list()
{
    fcgi_list_addr* tmp;
    tmp = new(nothrow) fcgi_list_addr;
    if (!tmp)
    {
        fprintf(stderr, "<%s:%d> Error malloc()\n", __func__, __LINE__);
        exit(1);
    }

    tmp->next = NULL;

    return tmp;
}
//======================================================================
int read_conf_file(const char* path_conf)
{
    String s, ss, nameFile;
    char buf[512];
    nameFile << path_conf;
    nameFile << "/server.conf";
    fcgi_list_addr* prev = NULL;

    ifstream fconf(nameFile.str(), ios::binary);
    if (!fconf.is_open())
    {
        cerr << __func__ << "(): Error create conf file (" << nameFile.str() << ")\n";
        cin.get();
        exit(1);
    }

    while (!fconf.eof())
    {
        ss.clear();
        fconf.getline(buf, sizeof(buf));
        ss << buf;
        ss >> s;
        if (s[0] == '#')
            continue;
//  cout << (ss.str()+ss.get_p()) << "\n";
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
            utf8_to_utf16(stmp, c.wCgiDir);
        }
        else if (s == "LogPath")
        {
            string stmp;
            ss >> stmp;
            utf8_to_utf16(stmp, c.wLogDir);
        }
        else if (s == "ListenBacklog")
            ss >> c.ListenBacklog;
        else if (s == "MaxRequests")
            ss >> c.MaxRequests;
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
            utf8_to_utf16(stmp, c.wPerlPath);
        }
        else if (s == "PyPath")
        {
            string stmp;
            ss >> stmp;
            utf8_to_utf16(stmp, c.wPyPath);
        }
        else if (s == "PathPHP-CGI")
        {
            string stmp;
            ss >> stmp;
            utf8_to_utf16(stmp, c.wPathPHP_CGI);
        }
        else if (s == "PathPHP-FPM")
        {
            ss >> c.pathPHP_FPM;
        }
        else if (s == "UsePHP")
            ss >> c.usePHP;
        else if (s == "ShowMediaFiles")
            ss >> c.ShowMediaFiles;
        else if (s == "ClientMaxBodySize")
            ss >> c.ClientMaxBodySize;
        else if (s == "index")
        {
            while (!fconf.eof())
            {
                ss.clear();
                fconf.getline(buf, sizeof(buf));
                ss << buf;
                ss >> s;

                if ((s[0] == '#') || (s.len() == 0) || (s[0] == '{'))
                    continue;
                else if (s[0] == '}')
                    break;
               
                if (s == "index.html")
                    c.index_html = 'y';
                else if (s == "index.php")
                    c.index_php = 'y';
                else if (s == "index.pl")
                    c.index_pl = 'y';
                else if (s == "index.fcgi")
                    c.index_fcgi = 'y';
            }

            if (s[0] != '}')
            {
                cerr << "   Error read config file\n";
                cin.get();
                exit(1);
            }
        }
        
        else if (s == "fastcgi")
        {
            while (!fconf.eof())
            {
                ss.clear();
                fconf.getline(buf, sizeof(buf));
                ss << buf;
                ss >> s;
                if ((s[0] == '#') || (s.len() == 0) || (s[0] == '{'))
                    continue;
                else if (s[0] == '}')
                    break;

                if (!prev)
                {
                    prev = c.fcgi_list = create_fcgi_list();
                    wstring stmp;
                    utf8_to_utf16(s.str(), stmp);
                    c.fcgi_list->scrpt_name = stmp;

                    ss >> s;
                    utf8_to_utf16(s.str(), stmp);
                    c.fcgi_list->addr = stmp;
                }
                else
                {
                    fcgi_list_addr* tmp;
                    tmp = create_fcgi_list();
                    wstring stmp;
                    utf8_to_utf16(s.str(), stmp);
                    tmp->scrpt_name = stmp;

                    ss >> s;
                    utf8_to_utf16(s.str(), stmp);
                    tmp->addr = stmp;
                    prev->next = tmp;
                    prev = tmp;
                }
            }
            if (s[0] != '}')
            {
                cerr << "   Error read config file\n";
                cin.get();
                exit(1);
            }
        }
    }

    fconf.close();

    fcgi_list_addr* i = c.fcgi_list;
    for (; i; i = i->next)
    {
        wcerr << L"[" << i->scrpt_name.c_str() << L"] = [" << i->addr.c_str() << L"]\n";
    }
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
