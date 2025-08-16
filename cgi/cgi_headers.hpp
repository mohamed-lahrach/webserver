// cgi_headers.hpp
#ifndef CGI_HEADERS_HPP
#define CGI_HEADERS_HPP
#include <string>
#include <map>

struct CgiResult {
    int                         status;       // default 200
    std::map<std::string,std::string> headers;
    std::string                 body;
    std::string                 raw;          // for debugging
    CgiResult(): status(200) {}
};

// Parse "Header: value\r\n..." up to blank line.
// Handles "Status: 302 Found" specially.
inline void parse_cgi_headers(const std::string& out, CgiResult& res) {
    res.raw = out;
    size_t pos = 0, n = out.size();
    // find header/body boundary
    size_t hdr_end = out.find("\r\n\r\n");
    size_t sep = (hdr_end == std::string::npos) ? out.find("\n\n") : hdr_end;
    if (sep == std::string::npos) sep = n;

    // iterate header lines
    while (pos < sep) {
        size_t eol = out.find("\r\n", pos);
        if (eol == std::string::npos || eol > sep) eol = sep;
        std::string line = out.substr(pos, eol - pos);
        pos = (eol < n && eol + 2 <= n) ? eol + 2 : eol;

        if (line.empty()) break;

        size_t colon = line.find(':');
        if (colon == std::string::npos) continue; // ignore malformed
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);
        // trim leading space in val
        while (!val.empty() && (val[0] == ' ' || val[0] == '\t')) val.erase(0,1);

        // Status special-case
        if (key == "Status" || key == "Status-Header" || key == "Status-Text") {
            // e.g. "302 Found" or "404"
            int code = 0;
            for (size_t i=0;i<val.size() && val[i]>='0' && val[i]<='9';++i) {
                code = code*10 + (val[i]-'0');
            }
            if (code >= 100 && code <= 599) res.status = code;
        } else {
            res.headers[key] = val;
        }
    }

    // body
    if (sep == n) res.body.clear();
    else {
        size_t body_off = (hdr_end != std::string::npos) ? (hdr_end + 4) : (sep + 2);
        if (body_off <= n) res.body = out.substr(body_off);
    }

    // default content-type if missing
    if (res.headers.find("Content-Type") == res.headers.end())
        res.headers["Content-Type"] = "text/html";
}

#endif
