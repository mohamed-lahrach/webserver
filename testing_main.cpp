#include "config/Lexer.hpp"
#include "config/parser.hpp"
#include <vector>
#include <iostream>

// Add this function to convert TokenType enum to string
std::string tokenTypeToString(TokenType type)
{
    switch (type)
    {
    case SERVER_KEYWORD:
        return "SERVER_KEYWORD";
    case LOCATION_KEYWORD:
        return "LOCATION_KEYWORD";
    case LISTEN_KEYWORD:
        return "LISTEN_KEYWORD";
    case SERVER_NAME_KEYWORD:
        return "SERVER_NAME_KEYWORD";
    case ROOT_KEYWORD:
        return "ROOT_KEYWORD";
    case INDEX_KEYWORD:
        return "INDEX_KEYWORD";
    case ERROR_PAGE_KEYWORD:
        return "ERROR_PAGE_KEYWORD";
    case ALLOWED_METHODS_KEYWORD:
        return "ALLOWED_METHODS_KEYWORD";
    case AUTOINDEX_KEYWORD:
        return "AUTOINDEX_KEYWORD";
    case CLIENT_MAX_BODY_SIZE_KEYWORD:
        return "CLIENT_MAX_BODY_SIZE_KEYWORD";
    case LEFT_BRACE:
        return "LEFT_BRACE";
    case RIGHT_BRACE:
        return "RIGHT_BRACE";
    case SEMICOLON:
        return "SEMICOLON";
    case COLON:
        return "COLON";
    case COMMA:
        return "COMMA";
    case SLASH:
        return "SLASH";
    case DOT:
        return "DOT";
    case STRING:
        return "STRING";
    case NUMBER:
        return "NUMBER";
    case IDENTIFIER:
        return "IDENTIFIER";
    case BOOLEAN_LITERAL:
        return "BOOLEAN_LITERAL";
    case SIZE:
        return "SIZE";
    case EOF_TOKEN:
        return "EOF_TOKEN";
    case UNKNOWN:
        return "UNKNOWN";
    case HTTP_METHOD_KEYWORD:
        return "HTTP_METHOD_KEYWORD";
    case PATH:
        return "PATH";
    default:
        return "UNKNOWN_TOKEN_TYPE";
    }
}

int main()
{
    Lexer lexer("./test_configs/default.conf");
    std::vector<Token> tokens = lexer.tokenizeAll();
    std::cout << "-------------------------" << std::endl;
    Parser parser(tokens);
    try
    {
        parser.parse();
         parser.getServers();
        std::cout << "Parsing completed successfully!" << std::endl;
    }
    catch (const std::runtime_error &e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}