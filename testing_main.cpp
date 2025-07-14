#include "config/Lexer.hpp"

#include <iostream>
#include <sstream>

int main() {
    std::cout << "Testing Lexer with String Content...\n";
    
    std::string simpleConfig = 
        "server {\n"
        "    listen 80;\n"
        "    server_name example.com;\n"
        "    root /var/www/html;\n"
        "}\n";
    
    std::cout << "Simple configuration:\n" << simpleConfig << "\n";
    
    // Create a Lexer instance with the string content
    Lexer lexer(simpleConfig);
    
    std::cout << "Tokenizing simple configuration...\n";
    std::cout << "-----------------------------------\n";

    
    return 0;
}