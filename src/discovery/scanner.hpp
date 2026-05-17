#pragma once

#include <string>
#include <vector>
#include <functional>
#include "config/config.hpp"
#include "db/crud.hpp"

namespace benchforge {

// ── ScanResult ────────────────────────────────────────────────────────────────
// Returned after a scan completes. Tells the caller what changed.
struct ScanResult {
    int new_models_found;       // newly registered models
    int models_deactivated;     // models whose files no longer exist
    int total_active_models;    // total active models after scan
    std::vector<std::string> errors; // non-fatal errors (unreadable dirs, etc.)
};

// ── Scanner ───────────────────────────────────────────────────────────────────
class Scanner {
public:
    Scanner(const Config& config, Crud& crud);

    // Scans all directories in config.scan_dirs for .gguf files.
    // Registers new models, deactivates missing ones.
    ScanResult scan();

    // Manually register a single model file by absolute path.
    // Returns the model id. Throws if file doesn't exist or isn't a .gguf.
    int register_model(const std::string& path);

    // Remove a manually registered model from active list.
    void unregister_model(int model_id);

private:
    const Config& config_;
    Crud&         crud_;

    // Scan a single directory, return all .gguf paths found.
    std::vector<std::string> scan_directory(const std::string& dir_path);

    // Build a Model struct from a file path (reads file size, derives name).
    Model build_model_from_path(const std::string& path);

    // Format bytes into human-readable string e.g. "4.1 GB".
    std::string format_file_size(int64_t bytes);

    // Derive a clean model name from a filename.
    // e.g. "gemma-4b-it-q4_k_m.gguf" -> "gemma-4b-it-q4_k_m"
    std::string derive_model_name(const std::string& filename);
};

} // namespace benchforge