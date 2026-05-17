#pragma once

#include <string>
#include <vector>
#include "db/models.hpp"
#include "config/config.hpp"

namespace benchforge {

// ── ExportFormat ──────────────────────────────────────────────────────────────
enum class ExportFormat {
    JSON,
    CSV
};

// ── ExportResult ──────────────────────────────────────────────────────────────
struct ExportResult {
    bool        success;
    std::string file_path;   // absolute path to written file
    std::string error;       // populated if success == false
};

// ── Exporter ──────────────────────────────────────────────────────────────────
class Exporter {
public:
    explicit Exporter(const Config& config);

    // Export a list of RunSummary objects to the configured export_dir.
    // Filename is auto-generated with a timestamp.
    // Returns the path to the written file.
    ExportResult exportRuns(const std::vector<RunSummary>& runs,
                            ExportFormat                   format);

    // Build JSON string from runs (used by API layer for inline response too).
    static std::string to_json(const std::vector<RunSummary>& runs);

    // Build CSV string from runs.
    static std::string to_csv(const std::vector<RunSummary>& runs);

private:
    const Config& config_;

    // Generate a timestamped filename e.g. "benchforge_export_20260518_142301.json"
    std::string generate_filename(ExportFormat format) const;

    // Ensure export directory exists, create if not.
    void ensure_export_dir() const;

    // Write content string to file path.
    void write_file(const std::string& path,
                    const std::string& content) const;
};

} // namespace benchforge