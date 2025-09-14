#include "post_handler.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h> 
#include <unistd.h>


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
	cgi_first_write = true;
	cgi_filename = "";
	std::cout << "PostHandler initialized." << std::endl;
}

PostHandler::~PostHandler()
{
	std::cout << "PostHandler destroyed." << std::endl;
}

RequestStatus PostHandler::save_request_body(const std::string &filename,
    const std::string &body, const LocationContext *loc)
{
	DIR* dir = opendir(loc->uploadStore.c_str());
    if (!dir)
    {
		std::cout << "Could not open upload directory: " << loc->uploadStore << std::endl;
        return (NOT_FOUND);
    }
	closedir(dir);
	if (access(loc->uploadStore.c_str(), W_OK) != 0)
	{
		std::cout << "Upload directory is not writable: " << loc->uploadStore << std::endl;
		return (FORBIDDEN);
	}
	std::string full_path;
	full_path = loc->uploadStore + "/" + filename;
	if(loc->uploadStore[loc->uploadStore.length() - 1] == '/')
		full_path = loc->uploadStore + filename;
	file_path = full_path;
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
		std::cout << "Could not open file for writing: " << full_path << std::endl;
        return (FORBIDDEN);
    }
    file.write(body.data(), body.size());
    file.close();
    
    std::cout << "✓ File saved: " << full_path << std::endl;
	return (POSTED_SUCCESSFULLY);
}

std::string PostHandler::extract_boundary(const std::string &content_type)
{
	size_t	pos;
	size_t	end_pos;
	std::string boundary_value;

	pos = content_type.find("boundary=");
	if (pos == std::string::npos)
	{
		std::cout << "Boundary not found in Content-Type header." << std::endl;
		return "";
	}
	
	pos += 9; // move past "boundary="
	
	// check if boundary is quoted
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
		// not quoted, find end by semicolon, space, or end of string
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
		std::cout << "ERROR: End quote for filename not found" << std::endl;
		return "";
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
std::string PostHandler::get_cgi_body() const
{
	std::string filename;
	if(cgi_filename.empty())
	  filename = "cgi_post_data.txt";
	else
	  filename = cgi_filename;
	std::string full_path = "/tmp/" + filename;
	
	// read CGI POST data from file instead of buffer
	std::ifstream file(full_path.c_str(), std::ios::binary);
	if (!file)
	{
		std::cout << "No CGI POST data file found at: " << full_path << std::endl;
		return "";
	}
	
	std::string content;
	std::string line;
	while (std::getline(file, line))
	{
		content += line + "\n";
	}
	
	// remove the last newline if it was added
	if (!content.empty() && content[content.length() - 1] == '\n')
	{
		content.erase(content.length() - 1);
	}
	
	file.close();
	std::cout << "Read " << content.size() << " bytes from CGI POST data file: " << full_path << std::endl;
	return content;
}

void PostHandler::clear_cgi_body()
{
	// Use stored filename if available, otherwise fallback to default
	std::string filename = NULL;
	if(cgi_filename.empty())
		filename = "cgi_post_data.txt";
	else
		filename = cgi_filename;
	std::string full_path = "/tmp/" + filename;
	
	// remove the CGI POST data file
	if (remove(full_path.c_str()) == 0)
	{
		std::cout << "CGI POST data file cleared: " << full_path << std::endl;
	}
	cgi_filename = "";
}

// CGI-specific save function - saves directly to /tmp
RequestStatus PostHandler::save_cgi_body(const std::string &data)
{
	std::string full_path = "/tmp/cgi_post_data.txt";
	std::ofstream file;

	if (cgi_first_write)
	{
		file.open(full_path.c_str(), std::ios::binary | std::ios::trunc);
		cgi_first_write = false;
	}
	else
	{
		file.open(full_path.c_str(), std::ios::binary | std::ios::app);
	}
	
	if (!file.is_open())
	{
		std::cout << "ERROR: Could not open CGI data file: " << full_path << std::endl;
		return FORBIDDEN;
	}
	
	file.write(data.data(), data.size());
	file.close();
	
	std::cout << "✓ CGI data saved: " << full_path << " (" << data.size() << " bytes)" << std::endl;
	return POSTED_SUCCESSFULLY;
}

// CGI-specific save function with filename from headers
RequestStatus PostHandler::save_cgi_body_with_filename(const std::string &data, const std::map<std::string, std::string> &headers)
{
	std::string filename = get_cgi_filename_from_headers(headers);
	std::string full_path = "/tmp/" + filename;

	if (cgi_first_write)
	{
		cgi_filename = filename;
	}
	
	std::ofstream file;
	if (cgi_first_write)
	{
		file.open(full_path.c_str(), std::ios::binary | std::ios::trunc);
		cgi_first_write = false;
	}
	else
	{
		file.open(full_path.c_str(), std::ios::binary | std::ios::app);
	}
	
	if (!file.is_open())
	{
		std::cout << "ERROR: Could not open CGI data file: " << full_path << std::endl;
		return FORBIDDEN;
	}
	
	file.write(data.data(), data.size());
	file.close();
	
	std::cout << "✓ CGI data saved: " << full_path << " (" << data.size() << " bytes)" << std::endl;
	return POSTED_SUCCESSFULLY;
}

std::string PostHandler::get_cgi_filename_from_headers(const std::map<std::string, std::string> &headers) const
{
	// Try to get filename from file_name exit in headers
	std::map<std::string, std::string>::const_iterator it = headers.find("x-file-name");
	if (it != headers.end() && !it->second.empty())
	{
		std::string filename = it->second;
		// Trim leading and trailing whitespace
		size_t start = filename.find_first_not_of(" \t\r\n");
		size_t end = filename.find_last_not_of(" \t\r\n");
		if (start != std::string::npos && end != std::string::npos)
		{
			filename = filename.substr(start, end - start + 1);
		}
		else if (start != std::string::npos)
		{
			filename = filename.substr(start);
		}
		
		std::cout << "Found filename in headers: '" << filename << "'" << std::endl;
		return filename;
	}
	
	// to default name
	std::cout << "No filename found in headers, using default" << std::endl;
	return "cgi_post_data.txt";
}
// CGI-specific methods
bool PostHandler::is_cgi_request(const LocationContext *loc, const std::string &requested_path) const
{
	if (!loc || loc->cgiExtensions.empty() || loc->cgiPaths.empty()) {
		return false;
	}
	
	// Strip query string for extension checking
	std::string path = requested_path;
	size_t query_pos = path.find('?');
	if (query_pos != std::string::npos)
	{
		path = path.substr(0, query_pos);
	}
	
	// Check if the requested path ends with any of the CGI extensions
	for (size_t i = 0; i < loc->cgiExtensions.size(); ++i)
	{
		const std::string& ext = loc->cgiExtensions[i];
		if (path.size() >= ext.size())
		{
			std::string file_ext = path.substr(path.size() - ext.size());
			if (file_ext == ext)
			{
				return true;
			}
		}
	}
	
	return false;
}

void PostHandler::remove_file_data(const std::string &full_path)
{
	std::ofstream file(full_path.c_str(), std::ios::trunc);
	if (!file)
	{
		std::cerr << "ERROR: Failed to clear file: " << full_path << std::endl;
		return;
	}
	// file is now empty
	file.close();
}
