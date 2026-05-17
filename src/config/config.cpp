#include "config.hpp"

#include <stdexcept>
#include <toml++/toml.hpp>

namespace benchforge {

Config default_config() {
    Config cfg;

    cfg.port                   = 7860;
    cfg.llama_bench_path       = "bin/llama-bench.exe";
    cfg.llama_perplexity_path  = "bin/llama-perplexity.exe";
    cfg.db_path                = "data/benchforge.db";
    cfg.open_browser           = true;

    cfg.recursive_scan         = true;

    cfg.default_preset         = "quick";
    cfg.threads                = 8;
    cfg.gpu_layers             = 99;
    cfg.repetitions            = 3;

    cfg.preset_quick    = { 128, 128, 1 };
    cfg.preset_thorough = { 512, 512, 5 };

    cfg.reference_file  = "reference/wiki.test.txt";
    cfg.export_dir      = "exports";

    // Built-in prompt presets as fallback
    cfg.prompt_presets = {
        { "short_qa",            "Short QA",            "Short factual question-answer",         64,   64  },
        { "short_programming",   "Short Programming",   "Short code completion",                 128,  128 },
        { "medium_reasoning",    "Medium Reasoning",    "Multi-step reasoning task",             256,  256 },
        { "medium_programming",  "Medium Programming",  "Medium complexity coding task",         512,  256 },
        { "long_architecture",   "Long Architecture",   "Large context architectural analysis",  1024, 512 },
        { "long_programming",    "Long Programming",    "Long codebase context task",            1024, 1024},
    };

    return cfg;
}

Config load_config(const std::string& path) {
    Config cfg = default_config(); // start with defaults, override from file

    toml::table tbl;
    try {
        tbl = toml::parse_file(path);
    } catch (const toml::parse_error& e) {
        throw std::runtime_error(
            std::string("Failed to parse config.toml: ") + e.description().data()
        );
    }

    // [general]
    if (auto v = tbl["general"]["port"].value<int>())               cfg.port                  = *v;
    if (auto v = tbl["general"]["llama_bench_path"].value<std::string>())      cfg.llama_bench_path      = *v;
    if (auto v = tbl["general"]["llama_perplexity_path"].value<std::string>()) cfg.llama_perplexity_path = *v;
    if (auto v = tbl["general"]["db_path"].value<std::string>())    cfg.db_path               = *v;
    if (auto v = tbl["general"]["open_browser"].value<bool>())      cfg.open_browser          = *v;

    // [discovery]
    if (auto* arr = tbl["discovery"]["scan_dirs"].as_array()) {
        cfg.scan_dirs.clear();
        arr->for_each([&](auto&& el) {
            if constexpr (toml::is_string<decltype(el)>)
                cfg.scan_dirs.push_back(*el);
        });
    }
    if (auto v = tbl["discovery"]["recursive_scan"].value<bool>()) cfg.recursive_scan = *v;

    // [benchmark]
    if (auto v = tbl["benchmark"]["default_preset"].value<std::string>()) cfg.default_preset = *v;
    if (auto v = tbl["benchmark"]["threads"].value<int>())                cfg.threads        = *v;
    if (auto v = tbl["benchmark"]["gpu_layers"].value<int>())             cfg.gpu_layers     = *v;
    if (auto v = tbl["benchmark"]["repetitions"].value<int>())            cfg.repetitions    = *v;

    // [presets.quick]
    if (auto v = tbl["presets"]["quick"]["prompt_tokens"].value<int>())     cfg.preset_quick.prompt_tokens     = *v;
    if (auto v = tbl["presets"]["quick"]["generation_tokens"].value<int>()) cfg.preset_quick.generation_tokens = *v;
    if (auto v = tbl["presets"]["quick"]["repetitions"].value<int>())       cfg.preset_quick.repetitions       = *v;

    // [presets.thorough]
    if (auto v = tbl["presets"]["thorough"]["prompt_tokens"].value<int>())     cfg.preset_thorough.prompt_tokens     = *v;
    if (auto v = tbl["presets"]["thorough"]["generation_tokens"].value<int>()) cfg.preset_thorough.generation_tokens = *v;
    if (auto v = tbl["presets"]["thorough"]["repetitions"].value<int>())       cfg.preset_thorough.repetitions       = *v;

    // [prompts] -- override individual presets if defined in toml
    for (auto& preset : cfg.prompt_presets) {
        auto node = tbl["prompts"][preset.id];
        if (auto v = node["label"].value<std::string>())            preset.label             = *v;
        if (auto v = node["description"].value<std::string>())      preset.description       = *v;
        if (auto v = node["prompt_tokens"].value<int>())            preset.prompt_tokens     = *v;
        if (auto v = node["generation_tokens"].value<int>())        preset.generation_tokens = *v;
    }

    // [perplexity]
    if (auto v = tbl["perplexity"]["reference_file"].value<std::string>()) cfg.reference_file = *v;

    // [export]
    if (auto v = tbl["export"]["export_dir"].value<std::string>()) cfg.export_dir = *v;

    return cfg;
}

} // namespace benchforge