#pragma once

#include <string>
#include <memory>
#include <httplib.h>

#include "config/config.hpp"
#include "db/database.hpp"
#include "db/crud.hpp"
#include "discovery/scanner.hpp"
#include "benchmark/runner.hpp"
#include "export/exporter.hpp"

namespace benchforge {

// ── Server ────────────────────────────────────────────────────────────────────
// Owns the HTTP server and all application state.
// One instance lives for the lifetime of the application.
class Server {
public:
    Server(const Config& config,
           Database&     db,
           Crud&         crud,
           Scanner&      scanner,
           Runner&       runner,
           Exporter&     exporter);

    // Start listening. Blocks until stop() is called.
    // Opens the browser automatically if config.open_browser is true.
    void start();

    // Signal the server to stop.
    void stop();

private:
    const Config& config_;
    Database&     db_;
    Crud&         crud_;
    Scanner&      scanner_;
    Runner&       runner_;
    Exporter&     exporter_;

    httplib::Server http_;

    // Mount all route handlers onto http_.
    void mount_routes();

    // Serve frontend static files from the frontend/ directory.
    void mount_static_files();

    // Open the default browser to localhost:<port>.
    void open_browser() const;

    // Middleware: set common response headers (CORS, content type).
    void set_common_headers(httplib::Response& res) const;
};

} // namespace benchforge