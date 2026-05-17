#include "server.hpp"
#include "server/routes.hpp"

#include <iostream>
#include <filesystem>
#include <stdexcept>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

namespace fs = std::filesystem;

namespace benchforge {

Server::Server(const Config& config,
               Database&     db,
               Crud&         crud,
               Scanner&      scanner,
               Runner&       runner,
               Exporter&     exporter)
    : config_(config)
    , db_(db)
    , crud_(crud)
    , scanner_(scanner)
    , runner_(runner)
    , exporter_(exporter)
{}

// ── Public ────────────────────────────────────────────────────────────────────

void Server::start() {
    mount_static_files();
    mount_routes();

    std::cout << "[BenchForge] Server starting on http://localhost:"
              << config_.port << "\n";

    if (config_.open_browser) {
        // Small delay so server is ready before browser opens
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            open_browser();
        }).detach();
    }

    // Blocking call -- runs until stop() is called
    if (!http_.listen("localhost", config_.port)) {
        throw std::runtime_error(
            "Failed to bind to port " + std::to_string(config_.port) +
            ". Is another process using it?"
        );
    }
}

void Server::stop() {
    http_.stop();
}

// ── Private ───────────────────────────────────────────────────────────────────

void Server::mount_static_files() {
    // Serve frontend/ directory as static files
    std::string frontend_path = "frontend";

    if (!fs::exists(frontend_path)) {
        throw std::runtime_error(
            "Frontend directory not found at: " + frontend_path +
            "\nMake sure frontend/ is next to the executable."
        );
    }

    // Serve index.html at root
    http_.Get("/", [frontend_path](const httplib::Request&,
                                    httplib::Response& res) {
        std::ifstream file(frontend_path + "/index.html");
        if (!file.is_open()) {
            res.status = 404;
            res.set_content("index.html not found", "text/plain");
            return;
        }
        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        res.set_content(content, "text/html");
    });

    // Serve static assets (css, js)
    http_.set_mount_point("/", frontend_path);
}

void Server::mount_routes() {
    // Inject dependencies into routes via a shared context struct
    Routes routes(config_, crud_, scanner_, runner_, exporter_);
    routes.mount(http_);
}

void Server::open_browser() const {
#ifdef _WIN32
    std::string url = "http://localhost:" + std::to_string(config_.port);
    ShellExecuteA(nullptr, "open", url.c_str(),
                  nullptr, nullptr, SW_SHOWNORMAL);
#else
    // Linux/macOS fallback
    std::string cmd = "xdg-open http://localhost:" +
                      std::to_string(config_.port) + " 2>/dev/null &";
    std::system(cmd.c_str());
#endif
}

void Server::set_common_headers(httplib::Response& res) const {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

} // namespace benchforge