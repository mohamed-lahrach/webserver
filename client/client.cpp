#include "client.hpp"

// Constructor
Client::Client() : ip_address(""), connect_time(0), request_count(0)
{
    std::cout << "Client constructor called" << std::endl;
}

// Destructor
Client::~Client()
{
    std::cout << "Client destructor called" << std::endl;
}

// Getter methods
std::string Client::get_ip() const
{
    return ip_address;
}

time_t Client::get_connect_time() const
{
    return connect_time;
}

int Client::get_request_count() const
{
    return request_count;
}

// Setter methods
void Client::set_ip(const std::string& ip)
{
    ip_address = ip;
}

void Client::set_connect_time(time_t time)
{
    connect_time = time;
}

void Client::increment_request_count()
{
    request_count++;
}