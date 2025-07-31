#ifndef PARSER_HPP
#define PARSER_HPP

#include <vector>
#include "Lexer.hpp"
#include <sstream> // Add at top of your parser.cpp

class Parser
{
private:
    size_t current;
    std::vector<Token> tokens;

public:
    Parser(const std::vector<Token> &tokenStream);
    const Token &peek();
    const Token &advance();
    const Token &previous();
    bool isAtEnd();
    bool match(TokenType type);
    void parse();
    void parseServerBlock();
    void expect(TokenType type, const std::string &errorMessage);

    // Directive-specific parsing functions
    void parseListenDirective();
    void parseRootDirective();
    void parseIndexDirective();
    void parseServerNameDirective();
    void parseClientMaxBodySizeDirective();
};

#endif
