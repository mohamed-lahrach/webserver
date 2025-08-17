#ifndef HELPER_FUNCTIONS_HPP
#define HELPER_FUNCTIONS_HPP

#include <string>
#include <cstdlib>  // for std::strtol
#include <cctype>   // for std::isdigit

// Helper function declarations
bool isAllDigits(const std::string &s);
bool isValidPortNumber(const std::string &s);
bool isValidIPv4Octet(const std::string &s);
bool isValidIPv4(const std::string &ip);

#endif // HELPER_FUNCTIONS_HPP
