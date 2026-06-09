#pragma once

#include "types.hpp"
#include <string>
#include <string_view>

namespace Lunar {

constexpr std::string_view status_phrase(int status) {
    switch (status) {
    case 200:
        return "OK";
    case 201:
        return "Created";
    case 204:
        return "No Content";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 429:
        return "Too Many Requests";
    case 500:
        return "Internal Server Error";
    case 502:
        return "Bad Gateway";
    default:
        return "Unknown";
    }
}

std::string url_decode(const std::string &string);
std::string build_response(const LuaResponse &res);

} // namespace Lunar