#include "bootstrap.hpp"
#include "utils/logger.hpp"
#include <curl/curl.h>
#include <iostream>

namespace Lunar {

Bootstrap::Bootstrap() {
    Logger::init(); 
    
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
        std::cerr << "critical: Failed to initialize libcurl!\n";
        std::exit(EXIT_FAILURE);
    }
}

Bootstrap::~Bootstrap() {
    curl_global_cleanup();
}

} // namespace Lunar