#include "http.hpp"

namespace Lunar {

std::string build_response(const LuaResponse &res) {
    std::string http;

    size_t estimated_size = 64 + res.m_body.size(); // 64 bytes for status line & standard headers
    for (const auto &[k, v] : res.m_headers) {
        estimated_size += k.size() + v.size() + 4; // +4 for ": " and "\r\n"
    }

    http.reserve(estimated_size);

    http.append("HTTP/1.1 ")
        .append(std::to_string(res.m_status))
        .append(" ")
        .append(status_phrase(res.m_status))
        .append("\r\n");
    http.append("Content-Length: ").append(std::to_string(res.m_body.size())).append("\r\n");

    for (const auto &[k, v] : res.m_headers) {
        http.append(k).append(": ").append(v).append("\r\n");
    }

    http.append("\r\n").append(res.m_body);

    return http;
}

std::string url_decode(const std::string &string) {
    std::string out;
    out.reserve(string.size());
    for (size_t i = 0; i < string.size(); ++i) {
        if (string[i] == '+') {
            out += ' ';
        } else if (string[i] == '%' && i + 2 < string.size() &&
                   (std::isxdigit((unsigned char)string[i + 1]) != 0) &&
                   (std::isxdigit((unsigned char)string[i + 2]) != 0)) {
            auto hexval = [](unsigned char chr) -> unsigned char {
                if (chr >= '0' && chr <= '9') {
                    return chr - '0';
                }
                if (chr >= 'a' && chr <= 'f') {
                    return chr - 'a' + 10;
                }
                return chr - 'A' + 10;
            };
            out += static_cast<char>((hexval((unsigned char)string[i + 1]) << 4) |
                                     hexval((unsigned char)string[i + 2]));
            i += 2;
        } else {
            out += string[i];
        }
    }
    return out;
}

}; // namespace Lunar