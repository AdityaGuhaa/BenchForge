#pragma once

#include <httplib.h>
#include "config/config.hpp"
#include "db/crud.hpp"
#include "discovery/scanner.hpp"
#include "benchmark/runner.hpp"
#include "export/exporter.hpp"

namespace benchforge {

// ── Routes ────────────────────────────────────────────────────────────────────
// Holds references to all application services and mounts
// every API endpoint onto the provided httplib::Server instance.
//
// API surface:
//
//  GET  /api/models              -- list all active models
//  POST /api/models/scan         -- trigger a folder scan
//  POST /api/models/register     -- manually register a model by path
//  DELETE /api/models/:id        -- deactivate a model
//
//  GET  /api/configs             -- list prompt presets from config
//  POST /api/benchmark/run       -- start a benchmark session
//  GET  /api/benchmark/status    -- is a session running?
//  POST /api/benchmark/abort     -- abort current session
//  GET  /api/benchmark/stream    -- SSE stream of benchmark progress
//
//  GET  /api/runs                -- all run summaries
//  GET  /api/runs/:id            -- single run summary
//  DELETE /api/runs/:id          -- delete a run
//
//  POST /api/export/json         -- export selected runs as JSON file
//  POST /api/export/csv          -- export selected runs as CSV file
//
//  GET  /api/system              -- system info (CPU, GPU, RAM, VRAM)

class Routes {
public:
    Routes(const Config&  config,
           Crud&          crud,
           Scanner&       scanner,
           Runner&        runner,
           Exporter&      exporter);

    // Mount all routes onto the server instance.
    void mount(httplib::Server& http);

private:
    const Config& config_;
    Crud&         crud_;
    Scanner&      scanner_;
    Runner&       runner_;
    Exporter&     exporter_;

    // SSE broadcaster -- holds active SSE connections
    // Key: connection id, Value: SSE sink pointer
    std::mutex                                          sse_mutex_;
    std::vector<httplib::DataSink*>                     sse_sinks_;

    // Broadcast a progress event to all active SSE connections.
    void broadcast_sse(const ProgressEvent& event);

    // ── Route Handlers ────────────────────────────────────────────────────────
    void handle_get_models      (const httplib::Request&, httplib::Response&);
    void handle_scan_models     (const httplib::Request&, httplib::Response&);
    void handle_register_model  (const httplib::Request&, httplib::Response&);
    void handle_delete_model    (const httplib::Request&, httplib::Response&);

    void handle_get_configs     (const httplib::Request&, httplib::Response&);

    void handle_run_benchmark   (const httplib::Request&, httplib::Response&);
    void handle_benchmark_status(const httplib::Request&, httplib::Response&);
    void handle_abort_benchmark (const httplib::Request&, httplib::Response&);
    void handle_benchmark_stream(const httplib::Request&, httplib::Response&);

    void handle_get_runs        (const httplib::Request&, httplib::Response&);
    void handle_get_run         (const httplib::Request&, httplib::Response&);
    void handle_delete_run      (const httplib::Request&, httplib::Response&);

    void handle_export_json     (const httplib::Request&, httplib::Response&);
    void handle_export_csv      (const httplib::Request&, httplib::Response&);

    void handle_system_info     (const httplib::Request&, httplib::Response&);

    // ── Helpers ───────────────────────────────────────────────────────────────

    // Set JSON content type and CORS headers.
    void json_response(httplib::Response&  res,
                       int                 status,
                       const std::string&  body) const;

    // Build a JSON error response body.
    static std::string error_json(const std::string& message);

    // Build a JSON success response body with optional data field.
    static std::string ok_json(const std::string& data = "{}");

    // Serialize a Model to JSON object string.
    static std::string model_to_json(const Model& model);

    // Serialize a RunSummary to JSON object string.
    static std::string summary_to_json(const RunSummary& s);
};

} // namespace benchforge