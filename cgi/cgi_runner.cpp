// tiny_cgi_webserver.cpp — minimal CGI-correct demo (C++98)
// Build & run:
//   g++ -std=c++98 -Wall -Wextra -Werror tiny_cgi_webserver.cpp -o web && ./web
//
// Then open http://127.0.0.1:8080/
//
// Endpoints:
//   GET  /           -> simple home page with links/forms
//   GET  /cgi        -> runs ./cgi_demo.py with QUERY_STRING
//   POST /cgi        -> runs ./cgi_demo.py with stdin + CONTENT_* env
//
// Notes:
// - We pass proper CGI env: REQUEST_METHOD, QUERY_STRING, CONTENT_LENGTH,
//   CONTENT_TYPE, GATEWAY_INTERFACE, SERVER_PROTOCOL, SERVER_NAME, SERVER_PORT,
//   SCRIPT_NAME, PATH_INFO (empty here).
// - For GET, child's stdin is empty. For POST, we write the body to child's stdin.

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
#include <vector>
#include <iostream>

// ----------------------- small utilities -----------------------
static bool write_all(int fd, const char *p, size_t n)
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
static std::string read_exact(int fd, size_t n)
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
static std::string read_until_double_crlf(int fd)
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
static void split_headers_body(const std::string &raw, std::string &headers, std::string &body)
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
static std::vector<char *> vecp(const std::vector<std::string> &v)
{
    std::vector<char *> out;
    for (size_t i = 0; i < v.size(); ++i)
        out.push_back(const_cast<char *>(v[i].c_str()));
    out.push_back(NULL);
    return out;
}

// ----------------------- HTTP helpers -----------------------
static std::string build_http_response_200_html(const std::string &html)
{
    char lenbuf[64];
    std::sprintf(lenbuf, "%lu", (unsigned long)html.size());
    std::string r = "HTTP/1.1 200 OK\r\n";
    r += "Content-Type: text/html; charset=utf-8\r\n";
    r += "Content-Length: ";
    r += lenbuf;
    r += "\r\n";
    r += "Connection: close\r\n";
    r += "\r\n";
    r += html;
    return r;
}
static std::string build_http_response_404()
{
    const char *body = "<!doctype html><html><body><h1>404 Not Found</h1></body></html>";
    char lenbuf[64];
    std::sprintf(lenbuf, "%lu", (unsigned long)std::strlen(body));
    std::string r = "HTTP/1.1 404 Not Found\r\n";
    r += "Content-Type: text/html; charset=utf-8\r\n";
    r += "Content-Length: ";
    r += lenbuf;
    r += "\r\n";
    r += "Connection: close\r\n\r\n";
    r += body;
    return r;
}
static std::string wrap_cgi_into_http(const std::string &raw)
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

// ----------------------- Request parsing -----------------------
struct Request
{
    std::string method;
    std::string target;
    std::string path;
    std::string query;
    std::map<std::string, std::string> headers;
    std::string body;
};
static void split_path_query(const std::string &target, std::string &path, std::string &query)
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
static Request read_request(int cfd)
{
    Request req;
    std::string head = read_until_double_crlf(cfd);

    std::string::size_type sp1 = head.find(' ');
    std::string::size_type sp2 = (sp1 == std::string::npos) ? std::string::npos : head.find(' ', sp1 + 1);
    std::string::size_type eol = head.find("\r\n");
    if (sp1 != std::string::npos && sp2 != std::string::npos && eol != std::string::npos && sp2 < eol)
    {
        req.method = head.substr(0, sp1);
        req.target = head.substr(sp1 + 1, sp2 - (sp1 + 1));
    }
    std::string::size_type pos = eol + 2;
    while (pos < head.size())
    {
        std::string::size_type e = head.find("\r\n", pos);
        if (e == std::string::npos)
            break;
        if (e == pos)
        {
            pos += 2;
            break;
        }
        std::string line = head.substr(pos, e - pos);
        std::string::size_type c = line.find(':');
        if (c != std::string::npos)
        {
            std::string k = line.substr(0, c);
            std::string v = line.substr(c + 1);
            while (!v.empty() && (v[0] == ' ' || v[0] == '\t'))
                v.erase(0, 1);
            req.headers[k] = v;
        }
        pos = e + 2;
    }
    split_path_query(req.target, req.path, req.query);
    size_t cl = 0;
    if (req.headers.find("Content-Length") != req.headers.end())
    {
        cl = (size_t)std::strtoul(req.headers["Content-Length"].c_str(), NULL, 10);
    }
    if (cl > 0)
        req.body = read_exact(cfd, cl);
    return req;
}

// ----------------------- CGI runner (exec external script) -----------------------
static std::vector<std::string> build_cgi_env(const Request &req,
                                              const std::string &server_name,
                                              const std::string &server_port,
                                              const std::string &script_name)
{
    std::vector<std::string> env;

    env.push_back("REQUEST_METHOD=" + req.method);
    env.push_back("QUERY_STRING=" + req.query); // GET data lives here
    // For POST, body is on stdin; inform via CONTENT_LENGTH/TYPE if present:
    std::map<std::string, std::string>::const_iterator itCT = req.headers.find("Content-Type");
    std::map<std::string, std::string>::const_iterator itCL = req.headers.find("Content-Length");
    if (itCT != req.headers.end())
        env.push_back("CONTENT_TYPE=" + itCT->second);
    if (itCL != req.headers.end())
        env.push_back("CONTENT_LENGTH=" + itCL->second);

    env.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.push_back("SERVER_PROTOCOL=HTTP/1.1");
    env.push_back("SERVER_NAME=" + server_name);
    env.push_back("SERVER_PORT=" + server_port);

    env.push_back("SCRIPT_NAME=" + script_name); // logical mapping of the script
    env.push_back("PATH_INFO=");                 // none in this tiny demo

    // Minimal but useful extras:
    std::map<std::string, std::string>::const_iterator itHost = req.headers.find("Host");
    if (itHost != req.headers.end()) env.push_back("HTTP_HOST=" + itHost->second);
    std::map<std::string, std::string>::const_iterator itUA = req.headers.find("User-Agent");
    if (itUA != req.headers.end()) env.push_back("HTTP_USER_AGENT=" + itUA->second);
    std::map<std::string, std::string>::const_iterator itAcc = req.headers.find("Accept");
    if (itAcc != req.headers.end()) env.push_back("HTTP_ACCEPT=" + itAcc->second);

    return env;
}

static std::string run_cgi_exec(const Request &req,
                                const std::string &interpreter,   // e.g. "/usr/bin/python3"
                                const std::string &script_path,   // e.g. "./cgi_demo.py"
                                const std::string &server_name,   // "127.0.0.1"
                                const std::string &server_port,   // "8080"
                                const std::string &script_name)   // "/cgi"
{
    int A[2], B[2];
    if (pipe(A) == -1 || pipe(B) == -1)
    {
        perror("pipe");
        return "";
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        close(A[0]); close(A[1]);
        close(B[0]); close(B[1]);
        return "";
    }

    if (pid == 0)
    {
        // Child: stdin <- A[0], stdout -> B[1]
        if (dup2(A[0], STDIN_FILENO) == -1) _exit(112);
        if (dup2(B[1], STDOUT_FILENO) == -1) _exit(113);

        close(A[0]); close(A[1]);
        close(B[0]); close(B[1]);

        // Build argv
        std::vector<std::string> argvS;
        argvS.push_back(interpreter);
        argvS.push_back(script_path);

        std::vector<char *> argv = vecp(argvS);

        // Build envp
        std::vector<std::string> envS = build_cgi_env(req, server_name, server_port, script_name);
        std::vector<char *> envp = vecp(envS);

        execve(argv[0], &argv[0], &envp[0]);
        _exit(127); // exec failed
    }

    // Parent
    close(A[0]); // parent doesn't read from A
    close(B[1]); // parent doesn't write to B

    // For POST: write the body to child's stdin. For GET: no body written.
    if (req.method == "POST" && !req.body.empty())
    {
        (void)write_all(A[1], req.body.c_str(), req.body.size());
    }
    // Important: close child's stdin so it sees EOF and can proceed.
    close(A[1]);

    // Read child's stdout (CGI raw: headers + \r\n\r\n + body)
    std::string raw;
    char buf[4096];
    for (;;)
    {
        ssize_t n = read(B[0], buf, sizeof(buf));
        if (n <= 0) break;
        raw.append(buf, (size_t)n);
    }
    close(B[0]);

    int st = 0;
    (void)waitpid(pid, &st, 0);
    return raw;
}

// ----------------------- Router -----------------------
static std::string render_home()
{
    std::string html;
    html += "<!doctype html><html><head><meta charset='utf-8'><title>Tiny CGI Demo</title>";
    html += "<style>body{font-family:system-ui, sans-serif;max-width:760px;margin:2rem auto;padding:0 1rem}";
    html += "label,input,textarea{display:block;margin:.5rem 0}textarea{width:100%;height:120px}</style>";
    html += "</head><body>";
    html += "<h1>Tiny CGI Web Server (C++98) + External Python CGI</h1>";
    html += "<p>GET uses <code>QUERY_STRING</code>; POST sends the body to the CGI via <code>stdin</code>.</p>";

    html += "<h2>GET form (QUERY_STRING)</h2>";
    html += "<form action='/cgi' method='get'><label>Text (GET):</label>";
    html += "<input name='text' value='hello from get'><button type='submit'>Send</button></form>";
    html += "<p>Or try direct link: <a href='/cgi?text=hello%20cgi&n=2'>/cgi?text=hello%20cgi&n=2</a></p>";

    html += "<h2>POST form (stdin body)</h2>";
    html += "<form action='/cgi' method='post'><label>Text (POST):</label>";
    html += "<textarea name='text'>hello from post</textarea>";
    html += "<button type='submit'>Send</button></form>";

    html += "</body></html>";
    return html;
}

static std::string handle_cgi(const Request &req)
{
    // Run an external Python script under real CGI rules
    const std::string interpreter = "/usr/bin/python3"; // adjust if needed
    const std::string script_path = "./cgi_demo.py";    // this file must be executable
    const std::string server_name = "127.0.0.1";
    const std::string server_port = "8080";
    const std::string script_name = "/cgi";

    std::string raw = run_cgi_exec(req, interpreter, script_path, server_name, server_port, script_name);
    if (raw.empty())
    {
        const char *msg = "HTTP/1.1 502 Bad Gateway\r\nContent-Type: text/plain\r\nContent-Length: 22\r\nConnection: close\r\n\r\nCGI child failed.\n";
        return std::string(msg);
    }
    return wrap_cgi_into_http(raw);
}

static std::string dispatch(const Request &req)
{
    if (req.method != "GET" && req.method != "POST")
    {
        return "HTTP/1.1 405 Method Not Allowed\r\n"
               "Content-Type: text/plain\r\n"
               "Content-Length: 18\r\n"
               "Connection: close\r\n\r\n"
               "Method Not Allowed";
    }
    if (req.path == "/" || req.path == "/index.html")
        return build_http_response_200_html(render_home());
    if (req.path == "/cgi")
        return handle_cgi(req);
    return build_http_response_404();
}

// ----------------------- main -----------------------
int main()
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
    {
        perror("socket");
        return 1;
    }

    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(8080);

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(s);
        return 1;
    }
    if (listen(s, 16) < 0)
    {
        perror("listen");
        close(s);
        return 1;
    }

    std::cout << "Tiny server on http://127.0.0.1:8080\n";
    std::cout << "Routes: / (form), /cgi (GET via QUERY_STRING, POST via stdin)\n";

    while (true)
    {
        int cfd = accept(s, NULL, NULL);
        if (cfd < 0)
        {
            perror("accept");
            continue;
        }
        Request req = read_request(cfd);
        std::string resp = dispatch(req);
        (void)write_all(cfd, resp.c_str(), resp.size());
        close(cfd);
    }
    close(s);
    return 0;
}
