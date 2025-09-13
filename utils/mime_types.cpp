#include "mime_types.hpp"
#include <algorithm>
#include <cctype>

MimeTypes::MimeTypes() 
{
        mime_map[".html"] = "text/html";
    mime_map[".htm"] = "text/html";
    mime_map[".css"] = "text/css";
    mime_map[".js"] = "application/javascript";
    mime_map[".json"] = "application/json";
    mime_map[".xml"] = "application/xml";
    mime_map[".txt"] = "text/plain";
    mime_map[".jpg"] = "image/jpeg";
    mime_map[".jpeg"] = "image/jpeg";
    mime_map[".png"] = "image/png";
    mime_map[".gif"] = "image/gif";
    mime_map[".bmp"] = "image/bmp";
    mime_map[".ico"] = "image/x-icon";
    mime_map[".svg"] = "image/svg+xml";
    mime_map[".pdf"] = "application/pdf";
    mime_map[".doc"] = "application/msword";
    mime_map[".zip"] = "application/zip";
    mime_map[".mp3"] = "audio/mpeg";
    mime_map[".wav"] = "audio/wav";
    mime_map[".mp4"] = "video/mp4";
    mime_map[".avi"] = "video/x-msvideo";
    mime_map[".woff"] = "font/woff";
    mime_map[".woff2"] = "font/woff2";
    mime_map[".ttf"] = "font/ttf";
}

std::string MimeTypes::get_file_extension(const std::string &file_path)
{
    size_t point_pos = file_path.find_last_of('.');
    if (point_pos == std::string::npos)
        return "";

    std::string extension = file_path.substr(point_pos);

    size_t i = 0;
    while (i < extension.length())
    {
        extension[i] = ::tolower(extension[i]);
        i++;
    }

    return extension;
}

std::string MimeTypes::get_mime_type(const std::string &file_path)
{
    std::string extension = get_file_extension(file_path);

    std::map<std::string, std::string>::iterator it = mime_map.find(extension);
    if (it != mime_map.end())
        return it->second;

    return "application/octet-stream";
}
