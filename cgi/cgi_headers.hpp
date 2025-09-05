#ifndef CGI_HEADERS_HPP
#define CGI_HEADERS_HPP

#include <string>
#include <map>
#include <vector>
#include <ctime>

struct CgiProcess {
    int pid;
    int input_fd;   // pipe to write to CGI stdin
    int output_fd;  // pipe to read from CGI stdout
    int client_fd;  // associated client
    std::string script_path;
    bool finished;
    std::string output_buffer;
    time_t start_time;
    
    CgiProcess() : pid(-1), input_fd(-1), output_fd(-1), client_fd(-1), finished(false), start_time(0) {}
};

#endif