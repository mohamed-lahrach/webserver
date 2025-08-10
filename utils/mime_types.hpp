#ifndef MIME_TYPES_HPP
#define MIME_TYPES_HPP

#include <string>
#include <map>

class MimeTypes
{
    std::map<std::string, std::string> mime_map;

public:
    MimeTypes();
    std::string get_mime_type(const std::string& file_path);
    std::string get_file_extension(const std::string& file_path);
};

#endif 
