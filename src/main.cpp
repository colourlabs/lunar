#include "pool/isolate_pool.hpp"
#include "server/server.hpp"
#include "utils/logger.hpp"
#include <span>
#include <string>
#include <uv.h>

int main(int argc, char **argv) {
    Logger::init();
    
    auto args = std::span(argv, static_cast<size_t>(argc));

    std::string worker = args.size() > 1 ? args[1] : "examples/hello/worker.lua";
    std::string std_path = args.size() > 2 ? args[2] : "lunar";
    int port = args.size() > 3 ? std::stoi(args[3]) : 8080;

    size_t threads = uv_available_parallelism();

    IsolatePool pool(worker, std_path, threads);

    uv_loop_t *loop = uv_default_loop();
    Server server(loop, &pool);

    if (!server.listen("0.0.0.0", port)) {
        Logger::error("failed to start server on port {}", port);
        return 1;
    }

    server.run();
    uv_loop_close(loop);
    return 0;
}