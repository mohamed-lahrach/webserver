#include "parser.hpp"
#include "Lexer.hpp"
#include <sstream>
#include <cstdlib>

static std::string toString(int number)
{
    std::ostringstream oss;
    oss << number;
    return oss.str();
}

Parser::Parser(const std::vector<Token> &tokenStream)
{
    current = 0;
    tokens = tokenStream;
}

const Token &Parser::peek()
{
    return tokens[current];
}
const Token &Parser::advance()
{
    if (current < tokens.size())
        current++;
    return previous();
}

const Token &Parser::previous()
{
    return tokens[current - 1];
}

bool Parser::isAtEnd()
{
    return current >= tokens.size();
}

bool Parser::match(TokenType type)
{
    if (isAtEnd())
        return false;
    if (tokens[current].type != type)
        return false;
    advance(); // Consume the token if it matches
    return true;
}

void Parser::parse()
{
    while (!isAtEnd())
    {
        if (peek().type == EOF_TOKEN)
            break;
        if (peek().type == SERVER_KEYWORD)
            parseServerBlock();
        else
        {
            std::ostringstream oss;
            oss << "Expected 'server' keyword at line " << peek().line;
            throw std::runtime_error(oss.str());
        }
    }
}

void Parser::parseServerBlock()
{
    expect(SERVER_KEYWORD, "Expected 'server' keyword");
    expect(LEFT_BRACE, "Expected '{' after 'server'");

    bool seenClientMaxBodySize = false;

    while (!isAtEnd() && peek().type != RIGHT_BRACE)
    {
        switch (peek().type)
        {
        case LISTEN_KEYWORD:
            parseListenDirective();
            break;
        case ROOT_KEYWORD:
            parseRootDirective();
            break;
        case CLIENT_MAX_BODY_SIZE_KEYWORD:
            if (seenClientMaxBodySize)
            {
                throw std::runtime_error("Duplicate 'client_max_body_size' directive at line " + toString(peek().line));
            }
            seenClientMaxBodySize = true;
            parseClientMaxBodySizeDirective();
            break;
        default:
            throw std::runtime_error("Unexpected directive at line " + toString(peek().line));
        }
    }

    expect(RIGHT_BRACE, "Expected '}' to close server block");
}

void Parser::expect(TokenType type, const std::string &errorMessage)
{
    if (!match(type))
    {
        std::ostringstream oss;
        oss << "Parser Error: " << errorMessage << " at line " << peek().line;
        throw std::runtime_error(oss.str());
    }
}

// Directive-specific parsing

void Parser::parseListenDirective()
{
    expect(LISTEN_KEYWORD, "Expected 'listen' directive");

    std::string host;
    std::string port;
    bool foundColon = false;

    while (!isAtEnd())
    {
        Token token = peek();
        if (token.type == SEMICOLON)
            break;

        if (token.type == DOT || token.type == COLON || token.type == NUMBER)
        {
            if (token.type == COLON)
            {
                foundColon = true;
            }
            else if (!foundColon)
            {
                host += token.value;
            }
            else
            {
                port += token.value;
            }
        }
        else
        {
            throw std::runtime_error("Unexpected token in listen directive at line " + toString(token.line));
        }

        advance();
    }

    expect(SEMICOLON, "Expected ';' after listen directive");

    std::cout << "[Listen] Host: " << host;
    if (!port.empty())
    {
        std::cout << ", Port: " << port;
    }
    std::cout << std::endl;
}

void Parser::parseRootDirective()
{
    expect(ROOT_KEYWORD, "Expected 'root' directive");

    if (peek().type != STRING)
    {
        throw std::runtime_error("Expected path after 'root' at line " + toString(peek().line));
    }

    std::string path = peek().value;
    advance();

    expect(SEMICOLON, "Expected ';' after root directive");

    std::cout << "[Root] " << path << std::endl;
}

void Parser::parseClientMaxBodySizeDirective()
{
    expect(CLIENT_MAX_BODY_SIZE_KEYWORD, "Expected 'client_max_body_size' directive");

    if (peek().type != NUMBER)
    {
        throw std::runtime_error("Expected numeric size value after 'client_max_body_size' at line " + toString(peek().line));
    }

    long long size = std::atoll(peek().value.c_str());
    advance();

    std::string unit;
    if (!isAtEnd() && peek().type == IDENTIFIER)
    {
        unit = peek().value;
        if (unit != "M" && unit != "K" && unit != "G")
        {
            throw std::runtime_error("Invalid unit for 'client_max_body_size' at line " + toString(peek().line));
        }
        advance();
    }

    expect(SEMICOLON, "Expected ';' after 'client_max_body_size' directive");

    // Convert to bytes for validation
    long long bytes = size;
    if (unit == "K") bytes *= 1024LL;
    else if (unit == "M") bytes *= 1024LL * 1024LL;
    else if (unit == "G") bytes *= 1024LL * 1024LL * 1024LL;

    std::cout << "[ClientMaxBodySize] Parsed: " << bytes << " bytes" << std::endl;

    // Example: impose a limit like 10 GB
    long long maxAllowed = 10LL * 1024LL * 1024LL * 1024LL;
    if (bytes > maxAllowed)
    {
        throw std::runtime_error("'client_max_body_size' exceeds allowed limit at line " + toString(peek().line));
    }
}
