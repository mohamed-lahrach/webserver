#include "post_handler.hpp"
// Add this function to convert size strings like "10M" to bytes
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
	is_chunked = false; // Initialize chunked transfer encoding flag
	std::cout << "PostHandler initialized." << std::endl;
}

PostHandler::~PostHandler()
{
	std::cout << "PostHandler destroyed." << std::endl;
}
void PostHandler::save_request_body(const std::string &filename,
	const std::string &body)

{
	std::ofstream file(filename.c_str(), std::ios::binary);
	// binary mode for any data
	if (!file)
	{
		throw std::runtime_error("Could not open file for writing");
	}
	file.write(body.data(), body.size());
	file.close();
}
std::string PostHandler::extract_boundary(const std::string &content_type)
{
	size_t	pos;

	pos = content_type.find("boundary=");
	if (pos == std::string::npos)
	{
		throw std::runtime_error("Boundary not found in Content-Type header");
	}
	return (content_type.substr(pos + 9)); // 9 is the length of "boundary="
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
		return "post_body.txt";
	}
	pos += 10;
	end_pos = body.find("\"", pos);
	if (end_pos == std::string::npos)
	{
		throw std::runtime_error("End quote for filename not found");
	}
	return (body.substr(pos, end_pos - pos));
}
