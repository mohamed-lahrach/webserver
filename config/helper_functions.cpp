#include "helper_functions.hpp"

// ---- helpers (file-local) ----
bool isAllDigits(const std::string &s) {
    if (s.empty()) return false;
    for (size_t i = 0; i < s.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    return true;
}

bool isValidPortNumber(const std::string &s) {
    if (!isAllDigits(s)) return false;
    // no leading '+' or other chars, allow "0"? usually no; use 1..65535
    long v = std::strtol(s.c_str(), 0, 10);
    return v >= 1 && v <= 65535;
}

bool isValidIPv4Octet(const std::string &s) {
    if (!isAllDigits(s)) return false;
    // disallow leading empty / too long
    if (s.size() > 3) return false;
    // "0" .. "255"
    long v = std::strtol(s.c_str(), 0, 10);
    if (v < 0 || v > 255) return false;
    // avoid leading zeros like "01"? NGINX tolerates, but keep it strict:
    if (s.size() > 1 && s[0] == '0') return false;
    return true;
}

bool isValidIPv4(const std::string &ip) {
    // split by '.'
    std::string part;
    int parts = 0;
    for (size_t i = 0; i <= ip.size(); ++i) {
        if (i == ip.size() || ip[i] == '.') {
            if (!isValidIPv4Octet(part)) return false;
            ++parts;
            part.clear();
        } else {
            part += ip[i];
        }
        if (parts > 4) return false;
    }
    return parts == 4;
}
