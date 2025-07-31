#ifndef LEXER_HPP        // 1. Include guard – prevents double inclusion
#define LEXER_HPP        //    of this header file in multiple translation units.

#include <string>        // 2. You need std::string for token text.
#include <cstddef>       //    Gives you size_t (unsigned integer type).
#include <fstream>   // std::ifstream
#include <sstream>   // std::ostringstream
#include <stdexcept> // std::runtime_error
#include <iostream>
#include <vector>  // Add this line

// ──────────────────────────────────────────────────────────────
// 3. Token kinds (enum class is safer, but a plain enum works fine)
enum TokenType {
    SERVER_KEYWORD,        // “server”
    LOCATION_KEYWORD,      // “location”
    LISTEN_KEYWORD,        // “listen”
    SERVER_NAME_KEYWORD,   // “server_name”
    ROOT_KEYWORD,          // “root”
    INDEX_KEYWORD,         // “index”
    ERROR_PAGE_KEYWORD,    // “error_page”
    ALLOWED_METHODS_KEYWORD, // “allowed_methods”
    AUTOINDEX_KEYWORD,     // “autoindex”
    CLIENT_MAX_BODY_SIZE_KEYWORD, // “client_max_body_size”

    HTTP_METHOD_KEYWORD, // GET, POST, etc. (optional helper)

    LEFT_BRACE, RIGHT_BRACE, // { }
    SEMICOLON,               // ;
    COLON,                   // :   (you’ll likely add this)
    COMMA,                   // ,   (optional, for lists)
    SLASH,                   // /
    DOT,                     // .   (optional, for file paths)

    STRING,                  // “/var/www”, “example.com” all none keywords
    NUMBER,                  // 80, 404
    IDENTIFIER,              // GET, POST, on, off
    BOOLEAN_LITERAL,         // on, off    (optional helper)
    SIZE,
    PATH,                    // /var/www/html (optional helper)

    EOF_TOKEN,               // End‑of‑file sentinel
    UNKNOWN                  // Anything unrecognised
};

// ──────────────────────────────────────────────────────────────
// 4. One concrete token with where it came from, for error messages.
struct Token {
    TokenType   type;   // the kind (from the enum)
    std::string value;  // exact text of the token
    int         line;   // 1‑based line number in the source file
    int         column; // 1‑based column where the token starts
};

// ──────────────────────────────────────────────────────────────
// 5. Lexer class – turns raw text into Token objects.
class Lexer {
private:
    std::string input;   // the whole config file in memory
    std::size_t position; // index of current char in ‘input’
    int line;            // current line (for diagnostics)
    int column;          // current column

public:
    // 5.1 Constructor – feed the entire file content
    explicit Lexer(const std::string& filePath);
    explicit Lexer(std::istream& in);              // optional helper
    ~Lexer();


    // 5.2 Primary API – get tokens
    
    std::vector<Token> tokenizeAll();


private:
    // main tokenization function 
    Token getNextToken(); // returns one Token and advances

    // 5.3 Helpers – make the top method readable
    bool  hasMoreTokens(); // true until we emit EOF_TOKEN
    char currentChar() const;   // char at ‘position’ or '\0' if EoF
    char peekChar() const;      // look‑ahead one char
    void advance();             // move position++, update line/column
    bool isAtEnd() const;


    void skipWhitespace();      // spaces, tabs, newlines
    void skipComment();         // ‘# … \n’

    Token readString();         // handles quoted or bare strings
    Token readNumber();         // handles integers
    Token readIdentifier();     // keywords / identifiers
    Token readSizeValue();      // handles size values like 100M, 1G, etc.

    TokenType getKeywordType(const std::string& word); // map to enum
    Token readWordOrPath();
};

#endif // LEXER_HPP        // 6. End of include guard
