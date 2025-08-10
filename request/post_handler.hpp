#ifndef POST_HANDLER_HPP
#define POST_HANDLER_HPP

#include <string>
#include <map>
#include "request_status.hpp"

class PostHandler
{
    
public:

    PostHandler();
    ~PostHandler();
    
    RequestStatus handle_post_request(
        const std::string& requested_path,
        const std::map<std::string, std::string>& http_headers,
        std::string& incoming_data,
        size_t expected_body_size
    );
    void parse_request_body(const std::string &body);
    void save_request_body(const std::string &filename, const std::string &body);


};

#endif 
