#include <iostream>
#include <span>
#include <string>

#include "runtime/isolate.h"

int main(int argc, char **argv) {
    auto args = std::span(argv, static_cast<size_t>(argc));
    std::string worker = args.size() > 1 ? args[1] : "examples/hello/worker.lua";

    Isolate isolate(worker);
    if (!isolate.ok()) {
        std::cerr << "Failed to load worker: " << isolate.error() << "\n";
        return 1;
    }

    LuaRequest req;
    req.m_method = "GET";
    req.m_path = "/hello";
    req.m_query = {{"name", "lunar"}};

    auto res = isolate.dispatch(req);
    if (!res) {
        std::cerr << "Dispatch failed: " << isolate.error() << "\n";
        return 1;
    }

    std::cout << "Status: " << res->m_status << "\n";
    for (auto &[k, v] : res->m_headers) {
        std::cout << k << ": " << v << "\n";
    }
    std::cout << "\n" << res->m_body << "\n";

    return 0;
}