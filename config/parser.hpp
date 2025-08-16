#ifndef PARSER_HPP
#define PARSER_HPP

#include <vector>
#include <sstream> // Add at top of your parser.cpp
#include "Lexer.hpp"
struct LocationContext
{
    std::string path;
    std::string root;
    std::vector<std::string> indexes;
    std::string autoindex;
    std::vector<std::string> allowedMethods;
    std::string returnDirective; // For return directive (could be just a file or "code file")
    std::string cgiExtension;
    std::string cgiPath;
    std::string uploadStore; // Directory where uploaded files are stored
};

typedef std::pair<std::vector<int>, std::string> ErrorPagePair;

struct ServerContext
{
    std::string host;
    std::string port;
    std::string root;
    std::vector<std::string> indexes;
    std::vector<ErrorPagePair> errorPages; // Much cleaner
    std::string clientMaxBodySize;
    std::string autoindex;
    std::vector<LocationContext> locations;
};

class Parser
{
private:
    size_t current;
    std::vector<Token> tokens;
    ServerContext currentServer;
    std::vector<ServerContext> servers;

    // Private helper methods
    const Token &peek();
    const Token &advance();
    const Token &previous();
    bool isAtEnd();
    bool match(TokenType type);
    void expect(TokenType type, const std::string &errorMessage);

    // Private parsing methods - Blocks
    void parseLocationBlock();
    void parseServerBlock();

    // Private parsing methods - Directive-specific parsing functions
    void parseHostDirective();
    void parsePortDirective();
    void parseRootDirective();
    void parseIndexDirective();
    void parseClientMaxBodySizeDirective();
    void parseErrorPageDirective();
    void parseAutoindexDirective();
    void parseReturnDirective();
    void parseReturnDirectiveInLocation(LocationContext& location);
    void parseCgiExtensionDirective(LocationContext& location);
    void parseCgiPathDirective(LocationContext& location);
    void parseUploadStoreDirective(LocationContext& location);

public:
    Parser(const std::vector<Token> &tokenStream);
    void parse();
    const std::vector<ServerContext> &getServers() const;
};

#endif
