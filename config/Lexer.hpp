#ifndef LEXER_HPP
#define LEXER_HPP

#include <string>
#include <vector>
#include <cstddef> // size_t
#include <fstream>
#include <sstream>
#include <stdexcept>

// Token kinds
enum TokenType
{
    // Keywords
    SERVER_KEYWORD,
    LOCATION_KEYWORD,
    LISTEN_KEYWORD,
    SERVER_NAME_KEYWORD,
    ROOT_KEYWORD,
    INDEX_KEYWORD,
    ERROR_PAGE_KEYWORD,
    ALLOWED_METHODS_KEYWORD,
    AUTOINDEX_KEYWORD,
    CLIENT_MAX_BODY_SIZE_KEYWORD,
    RETURN_KEYWORD,
    CGI_EXTENSION_KEYWORD,
    CGI_PATH_KEYWORD,
    HTTP_METHOD_KEYWORD, // GET, POST, PUT, DELETE, HEAD, OPTIONS, PATCH

    // Symbols
    LEFT_BRACE,
    RIGHT_BRACE, // { }
    SEMICOLON,   // ;
    COLON,       // :
    COMMA,       // ,
    DOT,         // . (rarely used now; dotted words parsed as STRING)

    // Literals
    STRING,          // paths, filenames, domains, "quoted"
    NUMBER,          // pure digits (no dot/letters)
    IDENTIFIER,      // (unused in this version – we classify non-keywords as STRING)
    BOOLEAN_LITERAL, // (if you decide to use it later)
    SIZE,            // (if you decide to use a distinct type later; not used here)

    EOF_TOKEN, // end-of-file
    UNKNOWN    // anything unrecognised
};

struct Token
{
    TokenType type;
    std::string value;
    int line;
    int column;
};

class Lexer
{
public:
    // Construct from a file path
    explicit Lexer(const std::string &filePath);
    // Construct from a stream (optional, handy for tests)
    explicit Lexer(std::istream &in);
    ~Lexer();

    Token getNextToken();
    bool hasMoreTokens();
    std::vector<Token> tokenizeAll();

private:
    // Core helpers
    char currentChar() const;
    char peekChar() const;
    void advance();
    bool isAtEnd() const;

    void skipWhitespace();
    void skipComment();

    // Scanners
    Token readNumber();     // pure digits
    Token readWordOrPath(); // quoted strings, /paths, dotted words, keywords
    Token readSizeValue();  // e.g., 1000000M / 512K (digits then letters, no dot)

    // Buffer/state
    std::string input;
    std::size_t position;
    int line;
    int column;
};

#endif // LEXER_HPP
