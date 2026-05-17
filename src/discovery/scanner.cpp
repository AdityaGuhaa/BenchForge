#include "scanner.hpp"

#include <filesystem>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;

namespace benchforge {

Scanner::Scanner(const Config& config, Crud& crud)
    : config_(config), crud_(crud)
{}

// ── Public ────────────────────────────────────────────────────────────────────

ScanResult Scanner::scan() {
    ScanResult result{0, 0, 0, {}};

    // Collect all .gguf paths found on disk
    std::vector<std::string> found_paths;
    for (const auto& dir : config_.scan_dirs) {
        try {
            auto paths = scan_directory(dir);
            found_paths.insert(found_paths.end(), paths.begin(), paths.end());
        } catch (const std::exception& e) {
            result.errors.push_back(
                "Error scanning " + dir + ": " + e.what()
            );
        }
    }

    // Register newly found models
    for (const auto& path : found_paths) {
        auto existing = crud_.get_model_by_path(path);
        if (!existing.has_value()) {
            try {
                Model m = build_model_from_path(path);
                crud_.insert_model(m);
                result.new_models_found++;
            } catch (const std::exception& e) {
                result.errors.push_back(
                    "Error registering " + path + ": " + e.what()
                );
            }
        }
    }

    // Deactivate models whose files no longer exist on disk
    auto active_models = crud_.get_all_models();
    for (const auto& model : active_models) {
        if (!fs::exists(model.path)) {
            crud_.deactivate_model(model.id);
            result.models_deactivated++;
        }
    }

    result.total_active_models = (int)crud_.get_all_models().size();
    return result;
}

int Scanner::register_model(const std::string& path) {
    // Validate file exists
    if (!fs::exists(path)) {
        throw std::runtime_error("File not found: " + path);
    }

    // Validate extension
    fs::path p(path);
    if (p.extension() != ".gguf") {
        throw std::runtime_error("Not a .gguf file: " + path);
    }

    // Check if already registered
    auto existing = crud_.get_model_by_path(path);
    if (existing.has_value()) {
        return existing->id; // already registered, return existing id
    }

    Model m = build_model_from_path(path);
    return crud_.insert_model(m);
}

void Scanner::unregister_model(int model_id) {
    crud_.deactivate_model(model_id);
}

// ── Private ───────────────────────────────────────────────────────────────────

std::vector<std::string> Scanner::scan_directory(const std::string& dir_path) {
    std::vector<std::string> paths;

    if (!fs::exists(dir_path)) {
        throw std::runtime_error("Directory does not exist: " + dir_path);
    }

    if (!fs::is_directory(dir_path)) {
        throw std::runtime_error("Path is not a directory: " + dir_path);
    }

    auto scan = [&](const fs::directory_entry& entry) {
        if (entry.is_regular_file() &&
            entry.path().extension() == ".gguf") {
            paths.push_back(entry.path().string());
        }
    };

    if (config_.recursive_scan) {
        for (const auto& entry : fs::recursive_directory_iterator(dir_path))
            scan(entry);
    } else {
        for (const auto& entry : fs::directory_iterator(dir_path))
            scan(entry);
    }

    // Sort for consistent ordering
    std::sort(paths.begin(), paths.end());
    return paths;
}

Model Scanner::build_model_from_path(const std::string& path) {
    fs::path p(path);

    Model m;
    m.id         = 0; // assigned by SQLite
    m.path       = path;
    m.name       = derive_model_name(p.filename().string());
    m.file_size  = (int64_t)fs::file_size(p);
    m.size_label = format_file_size(m.file_size);
    m.is_active  = true;

    return m;
}

std::string Scanner::format_file_size(int64_t bytes) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);

    if (bytes >= 1'000'000'000LL) {
        oss << (double)bytes / 1'000'000'000.0 << " GB";
    } else if (bytes >= 1'000'000LL) {
        oss << (double)bytes / 1'000'000.0 << " MB";
    } else {
        oss << (double)bytes / 1'000.0 << " KB";
    }

    return oss.str();
}

std::string Scanner::derive_model_name(const std::string& filename) {
    // Strip .gguf extension
    std::string name = filename;
    if (name.size() > 5 &&
        name.substr(name.size() - 5) == ".gguf") {
        name = name.substr(0, name.size() - 5);
    }
    return name;
}

} // namespace benchforge