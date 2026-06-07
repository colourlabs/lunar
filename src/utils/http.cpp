#include "http.hpp"

namespace HTTPUtils {

std::string build_response(const LuaResponse &res) {
    std::string http;
    
    size_t estimated_size = 64 + res.m_body.size(); // 64 bytes for status line & standard headers
    for (const auto &[k, v] : res.m_headers) {
        estimated_size += k.size() + v.size() + 4; // +4 for ": " and "\r\n"
    }
    
    http.reserve(estimated_size);

    http.append("HTTP/1.1 ").append(std::to_string(res.m_status)).append(" ").append(status_phrase(res.m_status)).append("\r\n");
    http.append("Content-Length: ").append(std::to_string(res.m_body.size())).append("\r\n");
    
    for (const auto &[k, v] : res.m_headers) {
        http.append(k).append(": ").append(v).append("\r\n");
    }
    
    http.append("\r\n").append(res.m_body);
    
    return http;
}

};