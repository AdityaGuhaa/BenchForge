#pragma once

#include <string>
#include <vector>

namespace benchforge {

// ── Prompt Preset ────────────────────────────────────────────────────────────
struct PromptPreset {
    std::string id;
    std::string label;
    std::string description;
    int         prompt_tokens;
    int         generation_tokens;
};

// ── Benchmark Preset ─────────────────────────────────────────────────────────
struct BenchmarkPreset {
    int prompt_tokens;
    int generation_tokens;
    int repetitions;
};

// ── Top-level Config ─────────────────────────────────────────────────────────
struct Config {
    // [general]
    int         port;
    std::string llama_bench_path;
    std::string llama_perplexity_path;
    std::string db_path;
    bool        open_browser;

    // [discovery]
    std::vector<std::string> scan_dirs;
    bool                     recursive_scan;

    // [benchmark]
    std::string default_preset;
    int         threads;
    int         gpu_layers;
    int         repetitions;

    // [presets]
    BenchmarkPreset preset_quick;
    BenchmarkPreset preset_thorough;

    // [prompts]
    std::vector<PromptPreset> prompt_presets;

    // [perplexity]
    std::string reference_file;

    // [export]
    std::string export_dir;
};

// ── Loader ───────────────────────────────────────────────────────────────────
// Loads config from a .toml file. Throws std::runtime_error on failure.
Config load_config(const std::string& path);

// Returns a Config populated with safe defaults (no file needed).
Config default_config();

} // namespace benchforge