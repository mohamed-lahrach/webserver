#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <ctime>
#include <iostream>

class Client
{
private:
    std::string ip_address;      // ADD THIS - missing member variable
    time_t connect_time;         // Keep same order as in constructor
    int request_count;           // Move this after connect_time

public:
    // Constructor
    Client();
    
    // Destructor
    ~Client();
    
    // Getter methods
    std::string get_ip() const;
    time_t get_connect_time() const;
    int get_request_count() const;
    
    // Setter methods
    void set_ip(const std::string& ip);
    void set_connect_time(time_t time);
    void increment_request_count();
};

#endif // CLIENT_HPP