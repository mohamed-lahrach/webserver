// Lexer.cpp – C++98‑compliant implementation of the NGINX‑style lexer
// ---------------------------------------------------------------
//  Build with:  g++ -Wall -Wextra -Werror -std=c++98 Lexer.cpp
// ---------------------------------------------------------------
#include "Lexer.hpp"
#include <cctype> // std::isspace, std::isdigit, std::isalpha
#include <string>
#include <iostream>

// toString helper for error messages


// ──────────────────────────────────────────────────────────────
//  Keyword lookup – simple if‑else chain (C++98 friendly)
//
static TokenType keywordLookup(const std::string &word)
{
    if (word == "server")
        return SERVER_KEYWORD;
    if (word == "location")
        return LOCATION_KEYWORD;
    if (word == "listen")
        return LISTEN_KEYWORD;
    if (word == "server_name")
        return SERVER_NAME_KEYWORD;
    if (word == "root")
        return ROOT_KEYWORD;
    if (word == "index")
        return INDEX_KEYWORD;
    if (word == "error_page")
        return ERROR_PAGE_KEYWORD;
    if (word == "allowed_methods")
        return ALLOWED_METHODS_KEYWORD;
    if (word == "autoindex")
        return AUTOINDEX_KEYWORD;
    if (word == "client_max_body_size")
        return CLIENT_MAX_BODY_SIZE_KEYWORD;
    if (word == "GET" || word == "POST" || word == "PUT" || word == "DELETE" || word == "HEAD" || word == "OPTIONS" || word == "PATCH")
        return HTTP_METHOD_KEYWORD;

    return STRING; // default to STRING for identifiers
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
    if (currentChar() == '\n')
    {
        ++line;
        column = 1;
    }
    else
    {
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
    int startCol = column;
    std::string value;
    while (std::isdigit(static_cast<unsigned char>(currentChar())))
    {
        value.push_back(currentChar());
        advance();
    }
    Token tok;
    tok.type = NUMBER;
    tok.value = value;
    tok.line = startLine;
    tok.column = startCol;
    return tok;
}

// ──────────────────────────────────────────────────────────────
//  Core token factory
//

Token Lexer::readWordOrPath()
{
    int startLine = line;
    int startColumn = column;
    std::string word;

    // Quoted string
    if (currentChar() == '"' || currentChar() == '\'')
    {
        char quote = currentChar();
        advance();

        while (!isAtEnd() && currentChar() != quote)
        {
            word += currentChar();
            advance();
        }

        if (currentChar() == quote)
            advance();
        else
            throw std::runtime_error("Unterminated quoted string");

        Token token;
        token.type = STRING;
        token.value = word;
        token.line = startLine;
        token.column = startColumn;
        return token;
    }

    // Path (starts with '/')
    if (currentChar() == '/')
    {
        while (!isAtEnd() && !std::isspace(currentChar()) &&
               currentChar() != ';' && currentChar() != '}')
        {
            word += currentChar();
            advance();
        }

        Token token;
        token.type = STRING; // Use STRING type for file paths
        token.value = word;
        token.line = startLine;
        token.column = startColumn;
        return token;
    }

    // Identifier or keyword
    while (!isAtEnd() && (std::isalnum(currentChar()) || currentChar() == '_' || currentChar() == '.'))
    {
        word += currentChar();
        advance();
    }

    Token token;
    token.type = keywordLookup(word);
    token.value = word;
    token.line = startLine;
    token.column = startColumn;
    return token;
}

Token Lexer::readSizeValue()
{
    std::string value;
    int startLine = line;
    int startCol = column;
    while (std::isdigit(static_cast<unsigned char>(currentChar())))
        value += currentChar(), advance();
    while (std::isalpha(static_cast<unsigned char>(currentChar())))
        value += currentChar(), advance();
    Token token;
    token.type = STRING;
    token.value = value;
    token.line = startLine;
    token.column = startCol;
    return token;
}
Token Lexer::getNextToken()
{
    while (true)
    {
        // 1. EOF
        if (currentChar() == '\0')
        {
            Token eofTok;
            eofTok.type = EOF_TOKEN;
            eofTok.line = line;
            eofTok.column = column;
            return eofTok;
        }

        // 2. Ignore blanks/comments
        if (std::isspace(static_cast<unsigned char>(currentChar())))
        {
            skipWhitespace();
            continue;
        }
        if (currentChar() == '#')
        {
            skipComment();
            continue;
        }

        // 3. Literals and identifiers
        if (isdigit(currentChar()))
        {
            // Look ahead for alpha after number to detect size units
            size_t temp = position;
            while (isdigit(input[temp]))
                temp++;
            if (isalpha(input[temp]))
                return readSizeValue(); // 1000000M → STRING
            return readNumber();        // 80, 443 → NUMBER
        }
        if (std::isalpha(currentChar()) || currentChar() == '_' || currentChar() == '/' ||
            currentChar() == '"' || currentChar() == '\'')
            return readWordOrPath(); // "server", "location", "/var/www/html" → STRING or IDENTIFIER
        

        // 4. Single‑char symbols
        Token tok;
        tok.line = line;
        tok.column = column;
        switch (currentChar())
        {
        case '{':
            tok.type = LEFT_BRACE;
            tok.value = "{";
            break;
        case '}':
            tok.type = RIGHT_BRACE;
            tok.value = "}";
            break;
        case ';':
            tok.type = SEMICOLON;
            tok.value = ";";
            break;
        case ':':
            tok.type = COLON;
            tok.value = ":";
            break;
        case ',':
            tok.type = COMMA;
            tok.value = ",";
            break;
        case '.':
            tok.type = DOT;
            tok.value = ".";
            break;
        default:
            tok.type = UNKNOWN;
            tok.value = std::string(1, currentChar());
            break;
        }
        advance();
        return tok;
    }
}

// ──────────────────────────────────────────────────────────────
//  Simple helper – any chars left?
//

bool Lexer::isAtEnd() const
{
    return position >= input.size();
}

bool Lexer::hasMoreTokens()
{
    return currentChar() != '\0';
}
std::vector<Token> Lexer::tokenizeAll()
{
    std::vector<Token> tokens;

    while (hasMoreTokens())
    {
        Token token = getNextToken();
        tokens.push_back(token);
        if (token.type == EOF_TOKEN)
            break;
    }

    return tokens;
}

// ──────────────────────────────────────────────────────────────

// --------------------------------------------------
// 1. Path‑based constructor
// --------------------------------------------------
Lexer::Lexer(const std::string &filePath)
{
    // std::ios::in | std::ios::binary
    // this mode ensures to open the file for reading, and don’t alter any byte when reading.
    std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot open config file: " + filePath);

    std::ostringstream buffer;
    buffer << file.rdbuf(); // read entire file
    input = buffer.str();
    position = 0; // reset position
    line = 1;     // reset line number
    column = 1;   // reset column number
    std::cout << "File content loaded from: " << filePath << std::endl;
}

// --------------------------------------------------
// 2. Stream constructor (unit‑test convenience)
// --------------------------------------------------
Lexer::Lexer(std::istream &in)
{
    std::ostringstream buffer;
    buffer << in.rdbuf();
    input = buffer.str();
    position = 0; // reset position
}

// --------------------------------------------------
// 3. Destructor (still empty; no manual resources)
// --------------------------------------------------
Lexer::~Lexer() {}
