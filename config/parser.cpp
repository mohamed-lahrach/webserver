#include "parser.hpp"
#include "Lexer.hpp"
static std::string toString(int number)
{
    std::ostringstream oss;
    oss << number;
    return oss.str();
}
Parser::Parser(const std::vector<Token> &tokenStream)
{
    tokens = tokenStream;
    current = 0;
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

// Checks if the current token matches the expected type.
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

    // For now just consume tokens until '}'
    while (!isAtEnd() && peek().type != RIGHT_BRACE)
    {
        switch (peek().type)
        {
        case LISTEN_KEYWORD:
            parseListenDirective();
            break;
        case SERVER_NAME_KEYWORD:
            std::cout << "[Server Name] " << peek().value << std::endl;
            advance(); // Consume the server name token
            break;
        case ROOT_KEYWORD:
            parseRootDirective();
            break;
        case INDEX_KEYWORD:
            std::cout << "[Index] " << peek().value << std::endl;
            advance(); // Consume the index token
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

// Directive-specific parsing functions

// Parses the listen directive, which can include an IP address and port.
void Parser::parseListenDirective() {
    expect(LISTEN_KEYWORD, "Expected 'listen' directive");

    std::string host;
    std::string port;
    bool foundColon = false;

    // Match IP address and port
    while (!isAtEnd()) {
        Token token = peek();
        if (token.type == SEMICOLON)
            break;

        if (token.type == DOT || token.type == COLON || token.type == NUMBER) {
            if (token.type == COLON) {
                foundColon = true;
            } else if (!foundColon) {
                host += token.value;
            } else {
                port += token.value;
            }
        } else {
            throw std::runtime_error("Unexpected token in listen directive at line " + toString(token.line));
        }

        advance();
    }
    
    expect(SEMICOLON, "Expected ';' after listen directive");
    
    // Display the parsed values
    std::cout << "[Listen] Host: " << host;
    if (!port.empty()) {
        std::cout << ", Port: " << port;
    }
    std::cout << std::endl;
}



void Parser::parseRootDirective() {
    expect(ROOT_KEYWORD, "Expected 'root' directive");

    if (peek().type != STRING) {
        throw std::runtime_error("Expected path after 'root' at line " + toString(peek().line));
    }

    std::string path = peek().value;
    // Ensure the path is a valid path
    validatePath(path);
    advance();

    expect(SEMICOLON, "Expected ';' after root directive");

    std::cout << "[Root] " << path << std::endl;
}
