#pragma once

#include <string>
#include <vector>
#include <optional>

namespace benchforge {

// ── RawBenchEntry ─────────────────────────────────────────────────────────────
// One entry from llama-bench's JSON output array.
// llama-bench outputs one object per (model, n_prompt, n_gen, repetition).
struct RawBenchEntry {
    std::string model_filename;
    int         n_prompt;           // prompt tokens
    int         n_gen;              // generation tokens
    double      t_pp_ms;            // prompt processing time (ms)
    double      t_tg_ms;            // token generation time (ms)
    int         n_pp;               // actual prompt tokens processed
    int         n_tg;               // actual generation tokens produced
    double      avg_pp_tps;         // average prompt tokens/sec
    double      avg_tg_tps;         // average generation tokens/sec
};

// ── ParsedMetrics ─────────────────────────────────────────────────────────────
// Final computed metrics derived from one or more RawBenchEntry objects
// (averaged across repetitions).
struct ParsedMetrics {
    double tokens_per_second;       // generation t/s (avg across reps)
    double prompt_tokens_per_sec;   // prompt processing t/s
    double gen_tokens_per_sec;      // generation t/s (same as tokens_per_second)
    double time_to_first_token_ms;  // estimated TTFT (prompt processing time / n_prompt)
    std::string raw_json;           // original JSON string for storage
};

// ── MetricsParser ─────────────────────────────────────────────────────────────
class MetricsParser {
public:
    // Parse the full JSON output string from llama-bench --output json.
    // Returns ParsedMetrics averaged across all repetitions.
    // Throws std::runtime_error if JSON is malformed or empty.
    static ParsedMetrics parse(const std::string& json_output);

    // Parse into raw entries without averaging (useful for debugging).
    static std::vector<RawBenchEntry> parse_raw(const std::string& json_output);

    // Estimate TTFT from prompt processing time and token count.
    // TTFT = t_pp_ms / n_pp (time per prompt token, as first token proxy)
    static double estimate_ttft_ms(double t_pp_ms, int n_pp);

private:
    static RawBenchEntry parse_entry(const std::string& json_output, int index);
};

} // namespace benchforge