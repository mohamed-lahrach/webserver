#include "cgi_headers.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>

CgiHandler::CgiHandler() {}

CgiHandler::~CgiHandler() {}

bool CgiHandler::write_all(int fd, const char *p, size_t n)
{
    while (n > 0)
    {
        ssize_t w = write(fd, p, n);
        if (w < 0)
            return false;
        p += (size_t)w;
        n -= (size_t)w;
    }
    return true;
}

std::string CgiHandler::read_exact(int fd, size_t n)
{
    std::string out;
    out.resize(n);
    size_t got = 0;
    while (got < n)
    {
        ssize_t r = read(fd, &out[got], n - got);
        if (r <= 0)
        {
            out.resize(got);
            break;
        }
        got += (size_t)r;
    }
    return out;
}

std::string CgiHandler::read_until_double_crlf(int fd)
{
    std::string out;
    char c;
    std::string tail;
    while (true)
    {
        ssize_t r = read(fd, &c, 1);
        if (r <= 0)
            break;
        out.push_back(c);
        tail.push_back(c);
        if (tail.size() > 4)
            tail.erase(0, tail.size() - 4);
        if (tail == "\r\n\r\n")
            break;
    }
    return out;
}

void CgiHandler::split_headers_body(const std::string &raw, std::string &headers, std::string &body)
{
    std::string::size_type p = raw.find("\r\n\r\n");
    size_t sep = 4;
    if (p == std::string::npos)
    {
        p = raw.find("\n\n");
        sep = 2;
    }
    if (p == std::string::npos)
    {
        headers = "";
        body = raw;
        return;
    }
    headers = raw.substr(0, p);
    body = raw.substr(p + sep);
}

std::vector<char *> CgiHandler::vecp(const std::vector<std::string> &v)
{
    std::vector<char *> out;
    for (size_t i = 0; i < v.size(); ++i)
        out.push_back(const_cast<char *>(v[i].c_str()));
    out.push_back(NULL);
    return out;
}

std::string CgiHandler::wrap_cgi_into_http(const std::string &raw)
{
    std::string headers, body;
    split_headers_body(raw, headers, body);
    std::string ct = "text/plain; charset=utf-8";
    std::string needle = "Content-Type:";
    std::string::size_type p = headers.find(needle);
    if (p != std::string::npos)
    {
        std::string line = headers.substr(p);
        std::string::size_type e = line.find('\n');
        if (e != std::string::npos)
            line = line.substr(0, e);
        std::string v = line.substr(needle.size());
        while (!v.empty() && (v[0] == ' ' || v[0] == '\t'))
            v.erase(0, 1);
        if (!v.empty())
            ct = v;
        if (!ct.empty() && ct[ct.size() - 1] == '\r')
            ct.erase(ct.size() - 1);
    }
    char lenbuf[64];
    std::sprintf(lenbuf, "%lu", (unsigned long)body.size());
    std::string r = "HTTP/1.1 200 OK\r\n";
    r += "Content-Type: " + ct + "\r\n";
    r += "Content-Length: ";
    r += lenbuf;
    r += "\r\n";
    r += "Connection: close\r\n\r\n";
    r += body;
    return r;
}

void CgiHandler::split_path_query(const std::string &target, std::string &path, std::string &query)
{
    std::string::size_type q = target.find('?');
    if (q == std::string::npos)
    {
        path = target;
        query = "";
    }
    else
    {
        path = target.substr(0, q);
        query = target.substr(q + 1);
    }
}

std::vector<std::string> CgiHandler::build_cgi_env(const CgiRequest &req,
                                                  const std::string &server_name,
                                                  const std::string &server_port,
                                                  const std::string &script_name)
{
    std::vector<std::string> env;
    
    env.push_back("REQUEST_METHOD=" + req.method);
    env.push_back("QUERY_STRING=" + req.query_string);
    env.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.push_back("SERVER_PROTOCOL=" + req.http_version);
    env.push_back("SERVER_NAME=" + server_name);
    env.push_back("SERVER_PORT=" + server_port);
    env.push_back("SCRIPT_NAME=" + script_name);
    env.push_back("PATH_INFO=");
    
    if (req.method == "POST")
    {
        char lenbuf[64];
        std::sprintf(lenbuf, "%lu", (unsigned long)req.body.size());
        env.push_back("CONTENT_LENGTH=" + std::string(lenbuf));
        
        std::map<std::string, std::string>::const_iterator it = req.headers.find("Content-Type");
        if (it != req.headers.end())
            env.push_back("CONTENT_TYPE=" + it->second);
        else
            env.push_back("CONTENT_TYPE=application/x-www-form-urlencoded");
    }
    
    return env;
}

std::string CgiHandler::execute_cgi(const CgiRequest &req,
                                   const std::string &interpreter,
                                   const std::string &script_path,
                                   const std::string &server_name,
                                   const std::string &server_port,
                                   const std::string &script_name)
{
    int pipe_in[2], pipe_out[2];
    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0)
        return "";

    pid_t pid = fork();
    if (pid < 0)
    {
        close(pipe_in[0]);
        close(pipe_in[1]);
        close(pipe_out[0]);
        close(pipe_out[1]);
        return "";
    }

    if (pid == 0)
    {
        // Child process
        close(pipe_in[1]);
        close(pipe_out[0]);
        
        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);
        
        close(pipe_in[0]);
        close(pipe_out[1]);

        std::vector<std::string> env_vec = build_cgi_env(req, server_name, server_port, script_name);
        std::vector<char *> env_ptrs = vecp(env_vec);

        std::vector<std::string> args;
        args.push_back(interpreter);
        args.push_back(script_path);
        std::vector<char *> arg_ptrs = vecp(args);

        execve(interpreter.c_str(), &arg_ptrs[0], &env_ptrs[0]);
        exit(1);
    }

    // Parent process
    close(pipe_in[0]);
    close(pipe_out[1]);

    if (req.method == "POST" && !req.body.empty())
    {
        write_all(pipe_in[1], req.body.c_str(), req.body.size());
    }
    close(pipe_in[1]);

    std::string output = read_until_double_crlf(pipe_out[0]);
    close(pipe_out[0]);

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        return output;
    
    return "";
}

bool CgiHandler::is_cgi_request(const std::string &path, const LocationContext *location)
{
    if (!location)
        return false;
    
    // Check if location has CGI configuration
    if (!location->cgiExtension.empty() && !location->cgiPath.empty())
    {
        // Check if path ends with the configured CGI extension
        if (path.length() >= location->cgiExtension.length())
        {
            std::string path_extension = path.substr(path.length() - location->cgiExtension.length());
            if (path_extension == location->cgiExtension)
                return true;
        }
    }
    
    // Fallback: check for common CGI patterns
    if (path.find("/cgi") != std::string::npos ||
        path.find(".py") != std::string::npos ||
        path.find(".pl") != std::string::npos ||
        path.find(".php") != std::string::npos)
    {
        return true;
    }
    
    return false;
}

std::string CgiHandler::handle_cgi_request(const CgiRequest &req, const LocationContext *location)
{
    if (!location)
        return "";
    
    std::string interpreter;
    std::string script_path;
    
    // Build script path - always use www as base directory
    if (!location->root.empty())
    {
        script_path = location->root + req.path;
    }
    else
    {
        script_path = "www" + req.path;
    }
    
    // Use configured CGI settings if available
    if (!location->cgiPath.empty())
    {
        interpreter = location->cgiPath;
    }
    else
    {
        // Fallback to auto-detection based on file extension
        if (req.path.find(".py") != std::string::npos)
        {
            interpreter = "/usr/bin/python3";
        }
        else if (req.path.find(".pl") != std::string::npos)
        {
            interpreter = "/usr/bin/perl";
        }
        else if (req.path.find(".php") != std::string::npos)
        {
            interpreter = "/usr/bin/php-cgi";
        }
        else
        {
            // Default to Python for /cgi paths
            interpreter = "/usr/bin/python3";
        }
    }
    
    std::string server_name = "127.0.0.1";
    std::string server_port = "3080"; // Use the port from your config
    std::string script_name = req.path;
    
    std::string raw_output = execute_cgi(req, interpreter, script_path, server_name, server_port, script_name);
    
    if (raw_output.empty())
    {
        return "HTTP/1.1 502 Bad Gateway\r\nContent-Type: text/plain\r\nContent-Length: 22\r\nConnection: close\r\n\r\nCGI execution failed.\n";
    }
    
    return wrap_cgi_into_http(raw_output);
}