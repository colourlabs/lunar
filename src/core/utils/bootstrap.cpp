#include "bootstrap.hpp"
#include "utils/logger.hpp"
#include <curl/curl.h>
#include <stdexcept>

namespace LunarCore {

Bootstrap::Bootstrap() {
    Logger::init();
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
        throw std::runtime_error("failed to initialize libcurl");
    }
    m_curl_initialized = true;
}

Bootstrap::~Bootstrap() {
    if (m_curl_initialized) {
        curl_global_cleanup();
    }
}

} // namespace LunarCore