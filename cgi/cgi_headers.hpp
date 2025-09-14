#ifndef CGI_HEADERS_HPP
#define CGI_HEADERS_HPP

#include <string>
#include <map>
#include <vector>

struct CgiProcess
{
    int pid;                   // pid of forked CGI
    int input_fd;              // parent writes request body -> child's stdin
    int output_fd;             // parent reads response <- child's stdout
    int client_fd;             // the original client socket
    std::string script_path;   // resolved script path
    bool finished;             // set when EOF + child reaped
    std::string output_buffer; // accumulates CGI stdout (headers+body)

    CgiProcess() : pid(-1), input_fd(-1), output_fd(-1), client_fd(-1), finished(false) {}
};

#endif