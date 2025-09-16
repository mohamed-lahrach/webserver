#ifndef CGI_HEADERS_HPP
#define CGI_HEADERS_HPP

#include <string>
#include <map>
#include <vector>
#include <ctime>

struct CgiProcess
{
    int pid;                 
    int input_fd;           
    int output_fd;     
    int client_fd;             
    std::string script_path;  
    bool finished;             
    std::string output_buffer;
    time_t start_time;       // When the CGI process started
    time_t last_activity;    // Last time we received data from this process

    CgiProcess() : pid(-1), input_fd(-1), output_fd(-1), client_fd(-1), finished(false) {
        start_time = time(NULL);
        last_activity = start_time;
    }
};

#endif