
#include "Lexer.hpp"
#include <cctype>
#include <string>
#include <iostream>

// ---------- keyword lookup ----------
static TokenType keywordLookup(const std::string &word)
{
    if (word == "server")
        return SERVER_KEYWORD;
    if (word == "location")
        return LOCATION_KEYWORD;
    if (word == "host")
        return HOST_KEYWORD;
    if (word == "port")
        return PORT_KEYWORD;
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
    if (word == "return")
        return RETURN_KEYWORD;
    if (word == "cgi_extension")
        return CGI_EXTENSION_KEYWORD;
    if (word == "cgi_path")
        return CGI_PATH_KEYWORD;
    if (word == "upload_store")
        return UPLOAD_STORE_KEYWORD;

    // HTTP methods as their own token (handy for allowed_methods)
    if (word == "GET" || word == "POST" || word == "PUT" ||
        word == "DELETE" || word == "HEAD" || word == "OPTIONS" || word == "PATCH")
        return HTTP_METHOD_KEYWORD;

    // Otherwise, we treat it as a STRING in this lexer design
    return STRING;
}

// ---------- core char helpers ----------
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

bool Lexer::isAtEnd() const
{
    return position >= input.size();
}

// ---------- whitespace/comments ----------
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

// ---------- scanners ----------
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

// digits+letters (no dot): e.g., 1000000M, 512K, 1G
Token Lexer::readSizeValue()
{
    int startLine = line;
    int startCol = column;
    std::string value;

    while (std::isdigit(static_cast<unsigned char>(currentChar())))
    {
        value.push_back(currentChar());
        advance();
    }
    while (std::isalpha(static_cast<unsigned char>(currentChar())))
    {
        value.push_back(currentChar());
        advance();
    }

    Token tok;
    tok.type = STRING; // we keep size-as-string; parser interprets it
    tok.value = value;
    tok.line = startLine;
    tok.column = startCol;
    return tok;
}

// quoted strings, /paths, and bare words (including dotted words like index.html, 404.html, example.com)
Token Lexer::readWordOrPath()
{
    int startLine = line;
    int startColumn = column;
    std::string word;

    // Quoted string
    if (currentChar() == '"' || currentChar() == '\'')
    {
        char quote = currentChar();
        advance(); // skip opening quote

        while (!isAtEnd() && currentChar() != quote)
        {
            word += currentChar();
            advance();
        }

        if (currentChar() == quote)
            advance(); // closing quote
        else
            throw std::runtime_error("Unterminated quoted string at line " + std::string("") /* avoid to_string in C++98 */);

        Token t;
        t.type = STRING;
        t.value = word;
        t.line = startLine;
        t.column = startColumn;
        return t;
    }

    // Path (starts with '/')
    if (currentChar() == '/')
    {
        while (!isAtEnd() &&
               !std::isspace(static_cast<unsigned char>(currentChar())) &&
               currentChar() != ';' && currentChar() != '}')
        {
            word += currentChar();
            advance();
        }
        Token t;
        t.type = STRING;
        t.value = word;
        t.line = startLine;
        t.column = startColumn;
        return t;
    }

    // Bare word (allow '.', '_', '-', ':' so that 'index.html', '404.html', 'www.example.com', 'http://example.com' stay whole)
    bool hasDot = false;
    while (!isAtEnd() &&
           (std::isalnum(static_cast<unsigned char>(currentChar())) ||
            currentChar() == '_' || currentChar() == '-' || currentChar() == '.' || currentChar() == ':' || currentChar() == '/'))
    {
        if (currentChar() == '.')
            hasDot = true;
        word += currentChar();
        advance();
    }

    Token t;
    t.value = word;
    t.line = startLine;
    t.column = startColumn;

    // Dotted words (index.html / 404.html / example.com) → STRING
    if (hasDot)
    {
        t.type = STRING;
    }
    else
    {
        // keywords / http methods (else STRING)
        t.type = keywordLookup(word);
    }
    return t;
}

// ---------- main token factory ----------
Token Lexer::getNextToken()
{
    for (;;)
    {
        // EOF
        if (currentChar() == '\0')
        {
            Token eofTok;
            eofTok.type = EOF_TOKEN;
            eofTok.value = "";
            eofTok.line = line;
            eofTok.column = column;
            return eofTok;
        }

        // Ignore whitespace and comments
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

        // Numbers vs sizes vs dotted words beginning with digit
        if (std::isdigit(static_cast<unsigned char>(currentChar())))
        {
            // Lookahead to classify
            std::size_t t = position;
            bool hasDot = false;
            bool hasAlpha = false;

            while (t < input.size())
            {
                unsigned char c = static_cast<unsigned char>(input[t]);
                if (std::isalnum(c) || c == '.' || c == '_' || c == '-')
                {
                    if (c == '.')
                        hasDot = true;
                    if (std::isalpha(c))
                        hasAlpha = true;
                    ++t;
                }
                else
                {
                    break;
                }
            }

            // digits + letters and NO dot → size literal (e.g., 1000M)
            if (hasAlpha && !hasDot)
            {
                return readSizeValue();
            }

            // dotted or alnum mix → treat as a single word (will become STRING)
            if (hasDot || hasAlpha)
            {
                return readWordOrPath();
            }

            // pure digits
            return readNumber();
        }

        // Words/paths/quoted strings
        if (std::isalpha(static_cast<unsigned char>(currentChar())) ||
            currentChar() == '_' || currentChar() == '-' ||
            currentChar() == '/' || currentChar() == '"' || currentChar() == '\'' || currentChar() == '.')
        {
            return readWordOrPath();
        }

        // Single-char symbols
        Token tok;
        tok.value = std::string(1, currentChar());
        tok.line = line;
        tok.column = column;
        switch (currentChar())
        {
        case '{':
            tok.type = LEFT_BRACE;
            break;
        case '}':
            tok.type = RIGHT_BRACE;
            break;
        case ';':
            tok.type = SEMICOLON;
            break;
        case '.':
            tok.type = DOT;
            break;
        default:
            tok.type = UNKNOWN;
            break;
        }
        advance();
        return tok;
    }
}

// ---------- bulk API ----------
bool Lexer::hasMoreTokens()
{
    return currentChar() != '\0';
}

std::vector<Token> Lexer::tokenizeAll()
{
    std::vector<Token> out;
    for (;;)
    {
        Token t = getNextToken();
        out.push_back(t);
        if (t.type == EOF_TOKEN)
            break;
    }
    return out;
}

// ---------- constructors / dtor ----------
Lexer::Lexer(const std::string &filePath)
    : position(0), line(1), column(1)
{
    std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot open config file: " + filePath);

    std::ostringstream buffer;
    buffer << file.rdbuf();
    input = buffer.str();

    std::cout << "File content loaded from: " << filePath << std::endl;
}

Lexer::Lexer(std::istream &in)
    : position(0), line(1), column(1)
{
    std::ostringstream buffer;
    buffer << in.rdbuf();
    input = buffer.str();
}

Lexer::~Lexer() {}
