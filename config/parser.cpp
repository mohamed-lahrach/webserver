#include "parser.hpp"
#include "Lexer.hpp"
#include "helper_functions.hpp"
#include <sstream>
#include <cstdlib>
#include <iostream>

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
const std::vector<ServerContext> &Parser::getServers() const
{
    return servers;
}
void Parser::parseServerBlock()
{
    expect(SERVER_KEYWORD, "Expected 'server' keyword");
    expect(LEFT_BRACE, "Expected '{' after 'server'");

    // Initialize a new ServerContext
    currentServer = ServerContext();

    bool seenClientMaxBodySize = false;

    while (!isAtEnd() && peek().type != RIGHT_BRACE)
    {
        switch (peek().type)
        {
        case HOST_KEYWORD:
            parseHostDirective();
            break;
        case PORT_KEYWORD:
            parsePortDirective();
            break;
        case ROOT_KEYWORD:
            parseRootDirective();
            break;
        case CLIENT_MAX_BODY_SIZE_KEYWORD:
            if (seenClientMaxBodySize)
                throw std::runtime_error("Duplicate 'client_max_body_size' directive at line " + toString(peek().line));
            seenClientMaxBodySize = true;
            parseClientMaxBodySizeDirective();
            break;
        case INDEX_KEYWORD:
            parseIndexDirective();
            break;
        case ERROR_PAGE_KEYWORD:
            parseErrorPageDirective();
            break;
        case AUTOINDEX_KEYWORD:
            parseAutoindexDirective();
            break;
        case LOCATION_KEYWORD:
            parseLocationBlock();
            break;
        default:
            throw std::runtime_error("Unexpected directive at line " + toString(peek().line));
        }
    }

    expect(RIGHT_BRACE, "Expected '}' to close server block");
    servers.push_back(currentServer); // Store the completed server context
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
void Parser::parseIndexDirective()
{
    advance(); // consume 'index'

    std::vector<std::string> indexFiles;

    while (!isAtEnd() && peek().type == STRING)
    {
        indexFiles.push_back(peek().value);
        advance(); // consume the identifier
    }

    if (indexFiles.empty())
        throw std::runtime_error("Expected at least one file after 'index' at line " + toString(peek().line));

    expect(SEMICOLON, "Expected ';' after index directive");

    // Store in currentServer
    currentServer.indexes = indexFiles;
}

void Parser::parseHostDirective()
{
    expect(HOST_KEYWORD, "Expected 'host' directive");

    if (peek().type != STRING)
        throw std::runtime_error("Expected IP address after 'host' at line " + toString(peek().line));

    std::string host = advance().value;
    if (!isValidIPv4(host))
        throw std::runtime_error("Invalid IPv4 address in host: '" + host +
                                 "' at line " + toString(peek().line));

    expect(SEMICOLON, "Expected ';' after host directive");

    // Store the host (keep existing port or default)
    currentServer.host = host;
    if (currentServer.port.empty())
        currentServer.port = "80";
}

void Parser::parsePortDirective()
{
    expect(PORT_KEYWORD, "Expected 'port' directive");

    if (peek().type != NUMBER)
        throw std::runtime_error("Expected port number after 'port' at line " + toString(peek().line));

    std::string port = advance().value;
    if (!isValidPortNumber(port))
        throw std::runtime_error("Invalid port number '" + port +
                                 "' in port directive at line " + toString(peek().line));

    expect(SEMICOLON, "Expected ';' after port directive");

    // Store the port (keep existing host or default)
    if (currentServer.host.empty())
        currentServer.host = "0.0.0.0";
    currentServer.port = port;
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

    // Store in currentServer instead of printing
    currentServer.root = path;
}

static bool isValidSizeWithUnit(const std::string &value)
{
    if (value.empty())
        return false;

    size_t len = value.length();
    if (len < 2)
        return false; // at least one digit + 1 unit

    char unit = value[len - 1];
    if (unit != 'M' && unit != 'K' && unit != 'G')
        return false;

    for (size_t i = 0; i < len - 1; ++i)
    {
        if (!std::isdigit(value[i]))
            return false;
    }

    return true;
}

void Parser::parseClientMaxBodySizeDirective()
{
    expect(CLIENT_MAX_BODY_SIZE_KEYWORD, "Expected 'client_max_body_size' directive");

    // Accept string token like "1000000M"
    if (peek().type != STRING)
    {
        throw std::runtime_error("Expected value like '1000M', '200K', or '1G' after 'client_max_body_size' at line " + toString(peek().line));
    }

    std::string value = peek().value;
    advance();

    // Validate format: digits + [M|K|G]
    if (!isValidSizeWithUnit(value))
    {
        throw std::runtime_error("Invalid format for 'client_max_body_size' at line " + toString(peek().line));
    }

    char unit = value[value.length() - 1];
    long long number = std::atoll(value.substr(0, value.length() - 1).c_str());

    long long bytes = number;
    if (unit == 'K')
        bytes *= 1024LL;
    else if (unit == 'M')
        bytes *= 1024LL * 1024LL;
    else if (unit == 'G')
        bytes *= 1024LL * 1024LL * 1024LL;

    expect(SEMICOLON, "Expected ';' after 'client_max_body_size' directive");

    // Enforce upper limit (example: 10 GB)
    long long maxAllowed = 10LL * 1024LL * 1024LL * 1024LL;
    if (bytes > maxAllowed)
    {
        throw std::runtime_error("'client_max_body_size' exceeds allowed limit at line " + toString(peek().line));
    }

    // Store in currentServer instead of printing
    currentServer.clientMaxBodySize = value;
}

void Parser::parseErrorPageDirective()
{
    advance(); // Skip 'error_page' keyword

    std::vector<int> errorCodes;

    // Collect all NUMBER tokens as error codes
    while (!isAtEnd() && peek().type == NUMBER)
    {
        int code = std::atoi(peek().value.c_str());
        errorCodes.push_back(code);
        advance();
    }

    // Ensure at least one error code was given
    if (errorCodes.empty())
    {
        throw std::runtime_error("Expected at least one error code for error_page directive at line " + toString(peek().line));
    }

    // Now expect the URI (as STRING)
    if (peek().type != STRING)
    {
        throw std::runtime_error("Expected URI after error codes at line " + toString(peek().line));
    }

    std::string uri = peek().value;
    advance(); // Consume URI

    expect(SEMICOLON, "Expected ';' after error_page directive");

    // Output to console for now
    currentServer.errorPages.push_back(std::make_pair(errorCodes, uri));
}

void Parser::parseAutoindexDirective()
{
    advance(); // Skip 'autoindex' keyword

    if (peek().type != STRING)
        throw std::runtime_error("Expected 'on' or 'off' after 'autoindex' at line " + toString(peek().line));

    std::string value = peek().value;
    if (value != "on" && value != "off")
        throw std::runtime_error("Invalid value for 'autoindex': expected 'on' or 'off' at line " + toString(peek().line));

    advance(); // Consume the value

    expect(SEMICOLON, "Expected ';' after 'autoindex' directive");

    currentServer.autoindex = value; // Store in currentServer
}

void Parser::parseLocationBlock()
{
    expect(LOCATION_KEYWORD, "Expected 'location' keyword");

    if (peek().type != STRING)
        throw std::runtime_error("Expected location path after 'location' at line " + toString(peek().line));

    std::string path = advance().value;
    expect(LEFT_BRACE, "Expected '{' after location path at line " + toString(peek().line));

    // Create new location context
    LocationContext location;
    location.path = path;

    while (!isAtEnd() && peek().type != RIGHT_BRACE)
    {
        Token token = advance();

        switch (token.type)
        {
        case ALLOWED_METHODS_KEYWORD:
        {
            while (peek().type == HTTP_METHOD_KEYWORD)
            {
                location.allowedMethods.push_back(advance().value);
            }
            expect(SEMICOLON, "Expected ';' after allowed_methods");
            break;
        }

        case ROOT_KEYWORD:
        {
            if (peek().type != STRING)
                throw std::runtime_error("Expected path after 'root' at line " + toString(peek().line));
            location.root = advance().value;
            expect(SEMICOLON, "Expected ';' after root");
            break;
        }

        case INDEX_KEYWORD:
        {
            while (peek().type == STRING)
            {
                location.indexes.push_back(advance().value);
            }
            expect(SEMICOLON, "Expected ';' after index");
            break;
        }

        case AUTOINDEX_KEYWORD:
        {
            if (peek().type != STRING || (peek().value != "on" && peek().value != "off"))
                throw std::runtime_error("Expected 'on' or 'off' after autoindex at line " + toString(peek().line));
            location.autoindex = advance().value;
            expect(SEMICOLON, "Expected ';' after autoindex");
            break;
        }

        case RETURN_KEYWORD:
        {
            parseReturnDirectiveInLocation(location);
            break;
        }

        case CGI_EXTENSION_KEYWORD:
        {
            parseCgiExtensionDirective(location);
            break;
        }

        case CGI_PATH_KEYWORD:
        {
            parseCgiPathDirective(location);
            break;
        }

        case UPLOAD_STORE_KEYWORD:
        {
            parseUploadStoreDirective(location);
            break;
        }

        default:
            throw std::runtime_error("Unknown directive '" + token.value + "' in location block at line " + toString(token.line));
        }
    }

    expect(RIGHT_BRACE, "Expected '}' to close location block at line " + toString(peek().line));

    // Store location in current server
    currentServer.locations.push_back(location);
}

void Parser::parseReturnDirectiveInLocation(LocationContext &location)
{
    std::string returnValue;

    if (peek().type == STRING)
    {
        returnValue = advance().value;
    }
    else
    {
        throw std::runtime_error("Expected status code or filename after 'return' at line " + toString(peek().line) +
                                 ". Got token type: " + toString(peek().type) + ", value: '" + peek().value + "'");
    }

    expect(SEMICOLON, "Expected ';' after return directive");

    location.returnDirective = returnValue;
}

void Parser::parseCgiExtensionDirective(LocationContext &location)
{
    // Clear any existing extensions
    location.cgiExtensions.clear();
    
    // Parse multiple extensions
    while (peek().type == STRING)
    {
        location.cgiExtensions.push_back(advance().value);
    }
    
    if (location.cgiExtensions.empty())
        throw std::runtime_error("Expected at least one file extension after 'cgi_extension' at line " + toString(peek().line));
    
    expect(SEMICOLON, "Expected ';' after cgi_extension");
}

void Parser::parseCgiPathDirective(LocationContext &location)
{
    // Clear any existing paths
    location.cgiPaths.clear();
    
    // Parse multiple interpreter paths
    while (peek().type == STRING)
    {
        location.cgiPaths.push_back(advance().value);
    }
    
    if (location.cgiPaths.empty())
        throw std::runtime_error("Expected at least one interpreter path after 'cgi_path' at line " + toString(peek().line));
    
    expect(SEMICOLON, "Expected ';' after cgi_path");
}

void Parser::parseUploadStoreDirective(LocationContext &location)
{
    if (peek().type != STRING)
        throw std::runtime_error("Expected upload directory path after 'upload_store' at line " + toString(peek().line));

    location.uploadStore = advance().value;
    expect(SEMICOLON, "Expected ';' after upload_store");
}
