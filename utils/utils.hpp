#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <map>

std::string url_decode(const std::string& encoded);
std::map<std::string, std::string> parse_query_string(const std::string& query_string);

#endif