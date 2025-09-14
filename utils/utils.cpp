#include "utils.hpp"
#include "../config/parser.hpp"
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <cctype>
#include <map>
#include <iostream>

std::string url_decode(const std::string &encoded)
{
    std::string result;

    for (size_t i = 0; i < encoded.length(); ++i)
    {
        if (encoded[i] == '%' && i + 2 < encoded.length())
        {
            std::string next_two_chars = encoded.substr(i + 1, 2);
            if (next_two_chars == "20")
            {
                result += ' ';
                i += 2;
            }
            else
            {
                result += encoded[i];
            }
        }
        else
            result += encoded[i];
    }

    return result;
}

std::map<std::string, std::string> parse_query_string(const std::string &query_string)
{
    std::map<std::string, std::string> params;

    if (query_string.empty())
        return params;
    std::stringstream ss(query_string);
    std::string pair;

    while (std::getline(ss, pair, '&'))
    {
        size_t equals_pos = pair.find('=');

        if (equals_pos != std::string::npos)
        {
            std::string key = url_decode(pair.substr(0, equals_pos));
            std::string value = url_decode(pair.substr(equals_pos + 1));
            params[key] = value;
        }
        else
        {
            std::string key = url_decode(pair);
            params[key] = "";
        }
    }

    return params;
}

std::string resolve_file_path(const std::string& request_path, LocationContext* location_config)
{
	if (!location_config)
		return "";

	std::string root = location_config->root;
	if (root == "/")
		root = ".";
	std::string relative_path = request_path;
	if (location_config->path != "/" && request_path.find(location_config->path) == 0)
	{
		relative_path = request_path.substr(location_config->path.length());
		std::cout << "Extracted relative path: " << relative_path << std::endl;

		if (!relative_path.empty() && relative_path[0] != '/')
			relative_path = "/" + relative_path;
		else if (relative_path.empty())
			relative_path = "/";
	}
	
	std::string file_path = root + relative_path;

	std::cout << "Resolved file path: " << file_path << std::endl;
	return file_path;
}