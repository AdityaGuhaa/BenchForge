#pragma once

#include <string>
#include <functional>
#include "config/config.hpp"
#include "db/models.hpp"

namespace benchforge {

// ── PerplexityResult ──────────────────────────────────────────────────────────
struct PerplexityResult {
    double      score;        // the PPL score (lower = better)
    double      uncertainty;  // +/- value reported by llama-perplexity
    bool        success;      // false if run failed
    std::string error;        // populated if success == false
    std::string raw_output;   // full stdout for debugging
};

// ── PerplexityRunner ──────────────────────────────────────────────────────────
class PerplexityRunner {
public:
    PerplexityRunner(const Config& config);

    // Run llama-perplexity on the given model using the reference file
    // specified in config. Blocking call -- can take 10-30 minutes.
    // progress_cb is called periodically with a status string.
    PerplexityResult run(
        const Model&                              model,
        std::function<void(const std::string&)>  progress_cb = nullptr
    );

private:
    const Config& config_;

    // Build llama-perplexity command arguments.
    std::vector<std::string> build_args(const Model& model) const;

    // Launch llama-perplexity subprocess and capture output.
    std::string launch_process(const std::vector<std::string>& args) const;

    // Parse PPL score from llama-perplexity stdout.
    // Output format: "Perplexity: 6.8341 +/- 0.0412"
    // Returns {score, uncertainty}. Throws if not found.
    std::pair<double, double> parse_output(const std::string& output) const;
};

} // namespace benchforge