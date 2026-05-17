#pragma once

#include <string>
#include <vector>
#include <ctime>

namespace benchforge {

// ── Model ─────────────────────────────────────────────────────────────────────
// Represents a discovered or manually registered .gguf model file.
struct Model {
    int         id;           // primary key (SQLite rowid)
    std::string name;         // derived from filename e.g. "gemma-4b-q4_k_m"
    std::string path;         // absolute path to .gguf file
    std::string size_label;   // human-readable e.g. "4.1 GB"
    int64_t     file_size;    // bytes
    std::string added_at;     // ISO 8601 timestamp
    bool        is_active;    // false = user deleted/moved file, kept for history
};

// ── BenchmarkConfig ───────────────────────────────────────────────────────────
// The exact parameters used for a benchmark run. Stored so results are
// reproducible and comparable across runs.
struct BenchmarkConfig {
    int         id;
    std::string preset_name;       // "quick", "thorough", or "custom"
    std::string prompt_type;       // e.g. "short_qa", "long_programming"
    int         prompt_tokens;
    int         generation_tokens;
    int         repetitions;
    int         threads;
    int         gpu_layers;
    bool        include_perplexity;
};

// ── BenchmarkRun ──────────────────────────────────────────────────────────────
// One complete benchmark execution for a single model with a single config.
struct BenchmarkRun {
    int         id;
    int         model_id;          // FK -> Model.id
    int         config_id;         // FK -> BenchmarkConfig.id
    std::string status;            // "pending", "running", "done", "failed"
    std::string started_at;        // ISO 8601
    std::string finished_at;       // ISO 8601
    std::string error_message;     // empty if no error
};

// ── BenchmarkResult ───────────────────────────────────────────────────────────
// The actual measured metrics from a completed BenchmarkRun.
struct BenchmarkResult {
    int    id;
    int    run_id;                 // FK -> BenchmarkRun.id

    // Throughput
    double tokens_per_second;      // generation tokens/sec (avg across reps)
    double prompt_tokens_per_sec;  // prompt processing speed
    double gen_tokens_per_sec;     // generation speed (same as tokens_per_second)

    // Latency
    double time_to_first_token_ms; // TTFT in milliseconds

    // Memory
    double ram_usage_mb;           // peak RAM during run
    double vram_usage_mb;          // peak VRAM during run (-1 if unavailable)

    // Quality (optional)
    double perplexity;             // -1.0 if not measured

    // Raw llama-bench JSON output (stored for debugging / re-parsing)
    std::string raw_output;
};

// ── RunSummary ────────────────────────────────────────────────────────────────
// Flattened view joining Model + BenchmarkRun + BenchmarkResult.
// Used by the API layer to return comparison data to the frontend.
struct RunSummary {
    int         run_id;
    std::string model_name;
    std::string model_path;
    std::string prompt_type;
    std::string preset_name;
    std::string status;
    std::string started_at;

    double tokens_per_second;
    double prompt_tokens_per_sec;
    double gen_tokens_per_sec;
    double time_to_first_token_ms;
    double ram_usage_mb;
    double vram_usage_mb;
    double perplexity;
};

} // namespace benchforge