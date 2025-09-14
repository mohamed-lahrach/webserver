#include "utils.hpp"
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <cctype>
#include <map>

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
std::string normalize_path(const std::string& path)
{
	if (path.empty())
		return "/";
	
	std::string result = path;
	
	for (size_t i = 0; i < result.length() - 1; ++i)
	{
		if (result[i] == '/' && result[i + 1] == '/')
		{
			result.erase(i, 1);
			--i;
		}
	}

	if (result[0] != '/')
		result = "/" + result;
	
	return result;
}