#include "config/Lexer.hpp"

#include <iostream>
#include <sstream>
#include <vector>

int main()
{

    // Example usage of Lexer
    Lexer lexer("./test_configs/default.conf");

    std::vector<Token> allTokens = lexer.tokenizeAll();

    for (std::vector<Token>::iterator it = allTokens.begin(); it != allTokens.end(); ++it)
    {
        std::cout << "Token: " << it->value
                  << " | Type: " << it->type
                  << " | Line: " << it->line
                  << " | Column: " << it->column << std::endl;
        std::cout << "-----------------------------------" << std::endl;
    }

    return 0;
}