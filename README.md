# server_nomad

This HTTP server for Windows OS was written to be used as a home server.

Server written on C++ (version C++11).

Compiled in Visual Studio 2019.

Tested on OS: Windows 7, Windows 10.

### Features
* One or more child processes (one or more threads in child process)
* Chunked transfer encoding
* CGI (languages for CGI scripts: PHP, Perl, Python)
* PHP-FPM
* Timeout for CGI
* Uses wide characters for local filesystem and utf-8 for clients
