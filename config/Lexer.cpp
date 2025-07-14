// Lexer.cpp – C++98‑compliant implementation of the NGINX‑style lexer
// ---------------------------------------------------------------
//  Build with:  g++ -Wall -Wextra -Werror -std=c++98 Lexer.cpp
// ---------------------------------------------------------------
#include "Lexer.hpp"

#include <cctype>      // std::isspace, std::isdigit, std::isalpha
#include <string>

// ──────────────────────────────────────────────────────────────
//  Keyword lookup – simple if‑else chain (C++98 friendly)
//
static TokenType keywordLookup(const std::string &word)
{
    if (word == "server")           return SERVER_KEYWORD;
    if (word == "location")         return LOCATION_KEYWORD;
    if (word == "listen")           return LISTEN_KEYWORD;
    if (word == "server_name")      return SERVER_NAME_KEYWORD;
    if (word == "root")             return ROOT_KEYWORD;
    if (word == "index")            return INDEX_KEYWORD;
    if (word == "error_page")       return ERROR_PAGE_KEYWORD;
    if (word == "allowed_methods")  return ALLOWED_METHODS_KEYWORD;
    if (word == "autoindex")        return AUTOINDEX_KEYWORD;
    return IDENTIFIER;
}

// ──────────────────────────────────────────────────────────────
//  Character helpers
//
char Lexer::currentChar() const
{
    return position < input.size() ? input[position] : '\0';
}

char Lexer::peekChar() const
{
    return (position + 1) < input.size() ? input[position + 1] : '\0';
}

void Lexer::advance()
{
    if (currentChar() == '\n') {
        ++line;
        column = 1;
    } else {
        ++column;
    }
    ++position;
}

// ──────────────────────────────────────────────────────────────
//  Whitespace / comment skipping
//
void Lexer::skipWhitespace()
{
    while (std::isspace(static_cast<unsigned char>(currentChar())))
        advance();
}

void Lexer::skipComment()
{
    if (currentChar() != '#')
        return;
    while (currentChar() != '\0' && currentChar() != '\n')
        advance();
    if (currentChar() == '\n')
        advance();
}

// ──────────────────────────────────────────────────────────────
//  Literal scanners
//
Token Lexer::readNumber()
{
    int startLine = line;
    int startCol  = column;
    std::string value;
    while (std::isdigit(static_cast<unsigned char>(currentChar()))) {
        value.push_back(currentChar());
        advance();
    }
    Token tok;
    tok.type   = NUMBER;
    tok.value  = value;
    tok.line   = startLine;
    tok.column = startCol;
    return tok;
}

Token Lexer::readIdentifier()
{
    int startLine = line;
    int startCol  = column;
    std::string word;
    while (std::isalnum(static_cast<unsigned char>(currentChar())) || currentChar() == '_') {
        word.push_back(currentChar());
        advance();
    }

    Token tok;
    tok.value  = word;
    tok.line   = startLine;
    tok.column = startCol;

    if (word == "on" || word == "off") {
        tok.type = BOOLEAN_LITERAL;
        return tok;
    }

    tok.type = keywordLookup(word);
    return tok;
}

Token Lexer::readString()
{
    int startLine = line;
    int startCol  = column;
    std::string result;

    bool quoted = false;
    char quoteChar = '\0';

    if (currentChar() == '"' || currentChar() == '\'') {
        quoted = true;
        quoteChar = currentChar();
        advance(); // consume opening quote
    }

    while (currentChar() != '\0') {
        if (quoted) {
            if (currentChar() == quoteChar) {
                advance(); // consume closing quote
                break;
            }
        } else {
            char c = currentChar();
            if (std::isspace(static_cast<unsigned char>(c)) || c=='{'||c=='}'||c==';'||c==':'||c==',')
                break;
        }

        // Support simple escaped quote or backslash inside quoted strings
        if (quoted && currentChar() == '\\' && (peekChar() == quoteChar || peekChar() == '\\'))
            advance(); // skip escape char but keep next char literal

        result.push_back(currentChar());
        advance();
    }

    Token tok;
    tok.type   = STRING;
    tok.value  = result;
    tok.line   = startLine;
    tok.column = startCol;
    return tok;
}

// ──────────────────────────────────────────────────────────────
//  Core token factory
//
Token Lexer::getNextToken()
{
    while (true) {
        // 1. EOF
        if (currentChar() == '\0') {
            Token eofTok;
            eofTok.type   = EOF_TOKEN;
            eofTok.line   = line;
            eofTok.column = column;
            return eofTok;
        }

        // 2. Ignore blanks/comments
        if (std::isspace(static_cast<unsigned char>(currentChar()))) {
            skipWhitespace();
            continue;
        }
        if (currentChar() == '#') {
            skipComment();
            continue;
        }

        // 3. Literals and identifiers
        if (std::isdigit(static_cast<unsigned char>(currentChar())))
            return readNumber();

        if (std::isalpha(static_cast<unsigned char>(currentChar())) || currentChar() == '_')
            return readIdentifier();

        if (currentChar() == '"' || currentChar() == '\'' || currentChar() == '/')
            return readString();

        // 4. Single‑char symbols
        Token tok;
        tok.line   = line;
        tok.column = column;
        switch (currentChar()) {
            case '{': tok.type = LEFT_BRACE;  tok.value = "{"; break;
            case '}': tok.type = RIGHT_BRACE; tok.value = "}"; break;
            case ';': tok.type = SEMICOLON;   tok.value = ";"; break;
            case ':': tok.type = COLON;       tok.value = ":"; break;
            case ',': tok.type = COMMA;       tok.value = ","; break;
            default:  tok.type = UNKNOWN;     tok.value = std::string(1, currentChar()); break;
        }
        advance();
        return tok;
    }
}

// ──────────────────────────────────────────────────────────────
//  Simple helper – any chars left?
//
bool Lexer::hasMoreTokens()
{
    return currentChar() != '\0';
}




// ──────────────────────────────────────────────────────────────

// --------------------------------------------------
// 1. Path‑based constructor
// --------------------------------------------------
Lexer::Lexer(const std::string& filePath)
{
    // std::ios::in | std::ios::binary 
    //this mode ensures to open the file for reading, and don’t alter any byte when reading.
    std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot open config file: " + filePath);

    std::ostringstream buffer;
    buffer << file.rdbuf();   // read entire file
    input = buffer.str();
    position = 0;             // reset position
    line = 1;                 // reset line number
    column = 1;               // reset column number
}

// --------------------------------------------------
// 2. Stream constructor (unit‑test convenience)
// --------------------------------------------------
Lexer::Lexer(std::istream& in)
{
    std::ostringstream buffer;
    buffer << in.rdbuf();
    input = buffer.str();
    position = 0;             // reset position
    
}

// --------------------------------------------------
// 3. Destructor (still empty; no manual resources)
// --------------------------------------------------
Lexer::~Lexer() {}
