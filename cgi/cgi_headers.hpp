#ifndef CGI_HEADERS_HPP
#define CGI_HEADERS_HPP

#include <string>
#include <map>
#include <vector>

struct CgiProcess
{
    int pid;                 
    int input_fd;           
    int output_fd;     
    int client_fd;             
    std::string script_path;  
    bool finished;             
    std::string output_buffer; 

    CgiProcess() : pid(-1), input_fd(-1), output_fd(-1), client_fd(-1), finished(false) {}
};

#endif