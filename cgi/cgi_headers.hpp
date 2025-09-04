#ifndef CGI_HEADERS_HPP
#define CGI_HEADERS_HPP

#include <string>
#include <vector>
#include <map>
#include "../request/request_status.hpp"
#include "../config/parser.hpp"

struct CgiRequest {
    std::string method;
    std::string path;
    std::string query_string;
    std::string http_version;
    std::map<std::string, std::string> headers;
    std::string body;
};

class CgiHandler
{
private:
    static bool write_all(int fd, const char *p, size_t n);
    static std::string read_exact(int fd, size_t n);
    static std::string read_until_double_crlf(int fd);
    static void split_headers_body(const std::string &raw, std::string &headers, std::string &body);
    static std::vector<char *> vecp(const std::vector<std::string> &v);
    static std::string wrap_cgi_into_http(const std::string &raw);
    static void split_path_query(const std::string &target, std::string &path, std::string &query);
    static std::vector<std::string> build_cgi_env(const CgiRequest &req,
                                                  const std::string &server_name,
                                                  const std::string &server_port,
                                                  const std::string &script_name);

public:
    CgiHandler();
    ~CgiHandler();
    
    static std::string execute_cgi(const CgiRequest &req,
                                  const std::string &interpreter,
                                  const std::string &script_path,
                                  const std::string &server_name,
                                  const std::string &server_port,
                                  const std::string &script_name);
    
    static bool is_cgi_request(const std::string &path, const LocationContext *location);
    static std::string handle_cgi_request(const CgiRequest &req, const LocationContext *location);
};

#endif