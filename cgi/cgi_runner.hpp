// cgi_runner.hpp
#ifndef CGI_RUNNER_HPP
#define CGI_RUNNER_HPP
#include <string>
#include <map>
#include "cgi_headers.hpp"

struct CgiConfig {
    std::string interpreter;   // e.g. "/usr/bin/python3"
    std::string script_path;   // absolute path to script
    std::string working_dir;   // optional chdir
    int         timeout_ms;    // e.g. 3000
};

bool run_cgi(const CgiConfig& cfg,
             const std::map<std::string,std::string>& env,
             const std::string& stdin_payload,
             CgiResult& out);

#endif
