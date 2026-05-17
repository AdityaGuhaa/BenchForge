#include "metrics.hpp"

#include <stdexcept>
#include <numeric>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace benchforge {

// ── Public ────────────────────────────────────────────────────────────────────

std::vector<RawBenchEntry> MetricsParser::parse_raw(const std::string& json_output) {
    if (json_output.empty()) {
        throw std::runtime_error("llama-bench produced no output");
    }

    json arr;
    try {
        arr = json::parse(json_output);
    } catch (const json::parse_error& e) {
        throw std::runtime_error(
            std::string("Failed to parse llama-bench JSON: ") + e.what()
        );
    }

    if (!arr.is_array() || arr.empty()) {
        throw std::runtime_error(
            "llama-bench JSON is not an array or is empty"
        );
    }

    std::vector<RawBenchEntry> entries;
    entries.reserve(arr.size());

    for (const auto& obj : arr) {
        RawBenchEntry entry{};

        // Model filename
        if (obj.contains("model_filename") && obj["model_filename"].is_string())
            entry.model_filename = obj["model_filename"].get<std::string>();

        // Token counts
        if (obj.contains("n_prompt") && obj["n_prompt"].is_number())
            entry.n_prompt = obj["n_prompt"].get<int>();
        if (obj.contains("n_gen") && obj["n_gen"].is_number())
            entry.n_gen = obj["n_gen"].get<int>();
        if (obj.contains("n_pp") && obj["n_pp"].is_number())
            entry.n_pp = obj["n_pp"].get<int>();
        if (obj.contains("n_tg") && obj["n_tg"].is_number())
            entry.n_tg = obj["n_tg"].get<int>();

        // Timing (ms)
        if (obj.contains("t_pp_ms") && obj["t_pp_ms"].is_number())
            entry.t_pp_ms = obj["t_pp_ms"].get<double>();
        if (obj.contains("t_tg_ms") && obj["t_tg_ms"].is_number())
            entry.t_tg_ms = obj["t_tg_ms"].get<double>();

        // Tokens per second
        // llama-bench field names: "avg_pp" and "avg_tg" (tokens/sec)
        if (obj.contains("avg_pp") && obj["avg_pp"].is_number())
            entry.avg_pp_tps = obj["avg_pp"].get<double>();
        if (obj.contains("avg_tg") && obj["avg_tg"].is_number())
            entry.avg_tg_tps = obj["avg_tg"].get<double>();

        entries.push_back(entry);
    }

    return entries;
}

ParsedMetrics MetricsParser::parse(const std::string& json_output) {
    auto entries = parse_raw(json_output);

    if (entries.empty()) {
        throw std::runtime_error("No benchmark entries found in llama-bench output");
    }

    // Average all numeric fields across repetitions
    double sum_pp_tps  = 0.0;
    double sum_tg_tps  = 0.0;
    double sum_t_pp_ms = 0.0;
    int    sum_n_pp    = 0;

    for (const auto& e : entries) {
        sum_pp_tps  += e.avg_pp_tps;
        sum_tg_tps  += e.avg_tg_tps;
        sum_t_pp_ms += e.t_pp_ms;
        sum_n_pp    += e.n_pp;
    }

    double count = (double)entries.size();

    ParsedMetrics m{};
    m.prompt_tokens_per_sec  = sum_pp_tps  / count;
    m.gen_tokens_per_sec     = sum_tg_tps  / count;
    m.tokens_per_second      = m.gen_tokens_per_sec; // primary metric
    m.time_to_first_token_ms = estimate_ttft_ms(
        sum_t_pp_ms / count,
        (int)(sum_n_pp / count)
    );
    m.raw_json = json_output;

    return m;
}

double MetricsParser::estimate_ttft_ms(double t_pp_ms, int n_pp) {
    // TTFT estimate: time to process a single prompt token
    // This is a proxy -- true TTFT requires instrumenting the model directly.
    // llama-bench doesn't expose per-token timing, so we divide total
    // prompt processing time by token count.
    if (n_pp <= 0) return 0.0;
    return t_pp_ms / (double)n_pp;
}

} // namespace benchforge