#ifndef PARSER_HPP
#define PARSER_HPP

#include <string>
#include <vector>
#include <map>

// Represents a location block inside a server
struct LocationConfig {
    std::string path;                         // "location /", path = "/"
    std::vector<std::string> allowedMethods;  // "allow_methods GET POST"
    std::string root;                         // Local root override
    std::string index;                        // Local index override
    bool autoindex;                           // autoindex on/off
    // You can add more like upload_store, client_max_body_size, etc.
};

// Represents a server block
struct ServerConfig {
    std::vector<std::string> listen;          // "listen IP:PORT"
    std::string serverName;                   // "server_name"
    std::string root;                         // "root"
    std::string index;                        // "index"
    std::map<int, std::string> errorPages;    // "error_page 404 /path"
    std::vector<LocationConfig> locations;    // All location blocks
    // Add any more fields if needed (e.g. client_max_body_size)
};

// Represents the full configuration
struct Config {
    std::vector<ServerConfig> servers;        // All server blocks
    // Optional: Add global settings like client_max_body_size
};

#endif // PARSER_HPP
