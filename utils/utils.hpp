#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <map>

struct LocationContext;

std::string url_decode(const std::string& encoded);
std::map<std::string, std::string> parse_query_string(const std::string& query_string);
std::string resolve_file_path(const std::string& request_path, LocationContext* location_config);

#endif