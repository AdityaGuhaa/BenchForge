#include "exporter.hpp"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <stdexcept>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace benchforge {

Exporter::Exporter(const Config& config)
    : config_(config)
{}

// ── Public ────────────────────────────────────────────────────────────────────

ExportResult Exporter::exportRuns(const std::vector<RunSummary>& runs,
                                   ExportFormat                   format) {
    ExportResult result{};
    result.success = false;

    if (runs.empty()) {
        result.error = "No runs provided for export";
        return result;
    }

    try {
        ensure_export_dir();

        std::string filename = generate_filename(format);
        std::string filepath = (fs::path(config_.export_dir) / filename).string();

        std::string content;
        if (format == ExportFormat::JSON) {
            content = to_json(runs);
        } else {
            content = to_csv(runs);
        }

        write_file(filepath, content);
        result.success   = true;
        result.file_path = filepath;

    } catch (const std::exception& e) {
        result.error = e.what();
    }

    return result;
}

// ── Static Serializers ────────────────────────────────────────────────────────

std::string Exporter::to_json(const std::vector<RunSummary>& runs) {
    json arr = json::array();

    for (const auto& r : runs) {
        json obj;
        obj["run_id"]                 = r.run_id;
        obj["model_name"]             = r.model_name;
        obj["model_path"]             = r.model_path;
        obj["prompt_type"]            = r.prompt_type;
        obj["preset_name"]            = r.preset_name;
        obj["status"]                 = r.status;
        obj["started_at"]             = r.started_at;
        obj["tokens_per_second"]      = r.tokens_per_second;
        obj["prompt_tokens_per_sec"]  = r.prompt_tokens_per_sec;
        obj["gen_tokens_per_sec"]     = r.gen_tokens_per_sec;
        obj["time_to_first_token_ms"] = r.time_to_first_token_ms;
        obj["ram_usage_mb"]           = r.ram_usage_mb;

        // Only include VRAM and perplexity if measured
        if (r.vram_usage_mb >= 0.0)
            obj["vram_usage_mb"] = r.vram_usage_mb;
        else
            obj["vram_usage_mb"] = nullptr;

        if (r.perplexity >= 0.0)
            obj["perplexity"] = r.perplexity;
        else
            obj["perplexity"] = nullptr;

        arr.push_back(obj);
    }

    return arr.dump(2); // pretty print with 2-space indent
}

std::string Exporter::to_csv(const std::vector<RunSummary>& runs) {
    std::ostringstream csv;

    // Header row
    csv << "run_id,"
        << "model_name,"
        << "model_path,"
        << "prompt_type,"
        << "preset_name,"
        << "status,"
        << "started_at,"
        << "tokens_per_second,"
        << "prompt_tokens_per_sec,"
        << "gen_tokens_per_sec,"
        << "time_to_first_token_ms,"
        << "ram_usage_mb,"
        << "vram_usage_mb,"
        << "perplexity\n";

    // Helper to escape CSV fields containing commas or quotes
    auto escape = [](const std::string& s) -> std::string {
        if (s.find(',') == std::string::npos &&
            s.find('"') == std::string::npos &&
            s.find('\n') == std::string::npos) {
            return s;
        }
        std::string escaped = "\"";
        for (char c : s) {
            if (c == '"') escaped += '"'; // double the quote
            escaped += c;
        }
        escaped += '"';
        return escaped;
    };

    auto fmt_double = [](double v, double sentinel = -1.0) -> std::string {
        if (v <= sentinel) return "";
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4) << v;
        return oss.str();
    };

    // Data rows
    for (const auto& r : runs) {
        csv << r.run_id                                    << ","
            << escape(r.model_name)                        << ","
            << escape(r.model_path)                        << ","
            << escape(r.prompt_type)                       << ","
            << escape(r.preset_name)                       << ","
            << escape(r.status)                            << ","
            << escape(r.started_at)                        << ","
            << fmt_double(r.tokens_per_second, -1.0)       << ","
            << fmt_double(r.prompt_tokens_per_sec, -1.0)   << ","
            << fmt_double(r.gen_tokens_per_sec, -1.0)      << ","
            << fmt_double(r.time_to_first_token_ms, -1.0)  << ","
            << fmt_double(r.ram_usage_mb, -1.0)            << ","
            << fmt_double(r.vram_usage_mb, -1.0)           << ","
            << fmt_double(r.perplexity, -1.0)              << "\n";
    }

    return csv.str();
}

// ── Private ───────────────────────────────────────────────────────────────────

std::string Exporter::generate_filename(ExportFormat format) const {
    // Get current UTC time for timestamp
    auto now     = std::chrono::system_clock::now();
    auto time_t  = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};

#ifdef _WIN32
    gmtime_s(&tm, &time_t);
#else
    gmtime_r(&time_t, &tm);
#endif

    std::ostringstream oss;
    oss << "benchforge_export_"
        << std::put_time(&tm, "%Y%m%d_%H%M%S");

    if (format == ExportFormat::JSON)
        oss << ".json";
    else
        oss << ".csv";

    return oss.str();
}

void Exporter::ensure_export_dir() const {
    fs::create_directories(config_.export_dir);
}

void Exporter::write_file(const std::string& path,
                           const std::string& content) const {
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + path);
    }
    file << content;
    if (file.fail()) {
        throw std::runtime_error("Failed to write to file: " + path);
    }
}

} // namespace benchforge