#include "post_handler.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h> 
#include <unistd.h> // For access()

// Static member definition
size_t PostHandler::total_received_size = 0;

size_t PostHandler::parse_max_body_size(const std::string &size_str)
{
	char	unit;
	size_t	value;

	unit = size_str[size_str.length() - 1];
	std::string number_str;
	if (unit == 'K'|| unit == 'M'|| unit == 'G')
	{
		number_str = size_str.substr(0, size_str.length() - 1);
	}
	// Convert number to integer
	value = atoi(number_str.c_str());
	switch (unit)
	{
	case 'K':
		return (value * 1024);
	case 'M':
		return (value * 1024 * 1024);
	case 'G':
		return (value * 1024 * 1024 * 1024);
	default:
		return (value);
	}
}
PostHandler::PostHandler()
{
	chunk_size = 0;
	first_chunk = true;
	total_received_size = 0;
	file_name_found = false;
	boundary_found = false;
	start_position = 0;
	data_start = false;
	std::cout << "PostHandler initialized." << std::endl;
}

PostHandler::~PostHandler()
{
	std::cout << "PostHandler destroyed." << std::endl;
}

// void PostHandler::reset_state()
// {
// 	total_received_size = 0;
// 	chunk_size = 0;
// 	first_chunk = true;
// 	file_name_found = false;
// 	boundary_found = false;
// 	start_position = 0;
// 	data_start = false;
// 	boundary.clear();
// 	file_name.clear();
// 	buffer_not_parser.clear();
// 	chunk_body_parser.clear();
// 	file_path.clear();
// 	std::cout << "PostHandler state reset." << std::endl;
// }

void PostHandler::save_request_body(const std::string &filename,
    const std::string &body, const LocationContext *loc)
{
	DIR* dir = opendir(loc->uploadStore.c_str());
    if (!dir)
    {
        throw std::runtime_error("Could not open upload directory");
    }
	closedir(dir);
	if (access(loc->uploadStore.c_str(), W_OK) != 0)
	{
		throw std::runtime_error("Upload directory is not writable");
	}
	std::string full_path;
	full_path = loc->uploadStore + "/" + filename;
	if(loc->uploadStore[loc->uploadStore.length() - 1] == '/')
		full_path = loc->uploadStore + filename;
	file_path = full_path;
    /// open file for append or create if it doesn't exist
    std::ofstream file;
	if(first_chunk)
	{
		file.open(full_path.c_str(), std::ios::binary | std::ios::trunc);
		first_chunk = false;
	}
	else
	{
		file.open(full_path.c_str(), std::ios::binary | std::ios::app);
	}
	std::cout << "Saving request body to: " << full_path << std::endl;
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open file for writing");
    }
    file.write(body.data(), body.size());
    file.close();
    
    std::cout << "✓ File saved: " << full_path << std::endl;
}

std::string PostHandler::extract_boundary(const std::string &content_type)
{
	size_t	pos;
	size_t	end_pos;
	std::string boundary_value;

	pos = content_type.find("boundary=");
	if (pos == std::string::npos)
	{
		throw std::runtime_error("Boundary not found in Content-Type header");
	}
	
	pos += 9; // Move past "boundary="
	
	// Check if boundary is quoted
	if (pos < content_type.length() && content_type[pos] == '"')
	{
		pos++; // Skip opening quote
		end_pos = content_type.find('"', pos);
		if (end_pos == std::string::npos)
		{
			// No closing quote found, take rest of string
			boundary_value = content_type.substr(pos);
		}
		else
		{
			boundary_value = content_type.substr(pos, end_pos - pos);
		}
	}
	else
	{
		// Not quoted, find end by semicolon, space, or end of string
		end_pos = content_type.find_first_of("; \t\r\n", pos);
		if (end_pos == std::string::npos)
		{
			boundary_value = content_type.substr(pos);
		}
		else
		{
			boundary_value = content_type.substr(pos, end_pos - pos);
		}
	}
	
	// Trim any remaining whitespace
	size_t start = boundary_value.find_first_not_of(" \t\r\n");
	size_t end = boundary_value.find_last_not_of(" \t\r\n");
	if (start != std::string::npos && end != std::string::npos)
	{
		boundary_value = boundary_value.substr(start, end - start + 1);
	}
	else if (start != std::string::npos)
	{
		boundary_value = boundary_value.substr(start);
	}
	
	boundary_found = true;
	std::cout << "Extracted boundary: '" << boundary_value << "'" << std::endl;
	return boundary_value;
}
std::string PostHandler::extract_filename(const std::string &body)
{
	size_t	pos;
	size_t	end_pos;

	pos = body.find("filename=\"");
	//default filename if not found
	if (pos == std::string::npos)
	{
		std::cout << "Filename not found in body, using default." << std::endl;
		return "post_body_default.txt";
	}
	pos += 10;
	end_pos = body.find("\"", pos);
	if (end_pos == std::string::npos)
	{
		throw std::runtime_error("End quote for filename not found");
	}
	file_name_found = true;
	return (body.substr(pos, end_pos - pos));
}
int PostHandler::parse_size(const ServerContext *cfg, std::string &incoming_data)
{
		if (cfg->clientMaxBodySize != "0"
		&& incoming_data.size() > parse_max_body_size(cfg->clientMaxBodySize))
	{
		std::cout << "ERROR: POST body size is too large!" << std::endl;
		std::cout << "Received body size: " << incoming_data.size() << std::endl;
		std::cout << "Max allowed body size: " << parse_max_body_size(cfg->clientMaxBodySize) << std::endl;
		return  0;
	}
	return 1;;
}
