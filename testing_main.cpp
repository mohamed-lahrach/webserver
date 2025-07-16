#include "config/Lexer.hpp"

#include <iostream>
#include <sstream>

int main() {
    
    // Example usage of Lexer
    Lexer lexer("./test_configs/default.conf");

    while (lexer.hasMoreTokens()) {
        Token token = lexer.getNextToken();
        std::cout << "Token: Type=" << token.type << ", Value='" << token.value 
                  << "', Line=" << token.line << ", Column=" << token.column << std::endl;
        std::cout << "----------------------------------" << std::endl;
    }
    
    return 0;
}