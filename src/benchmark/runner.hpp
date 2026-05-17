#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>

#include "config/config.hpp"
#include "db/crud.hpp"
#include "db/models.hpp"
#include "benchmark/metrics.hpp"
#include "utils/system_info.hpp"

namespace benchforge {

// ── BenchmarkRequest ──────────────────────────────────────────────────────────
// What the API layer sends to the runner to kick off a benchmark session.
struct BenchmarkRequest {
    std::vector<int> model_ids;     // models to benchmark (sequential)
    int              config_id;     // FK -> benchmark_configs row
    BenchmarkConfig  config;        // full config (pre-fetched by API layer)
};

// ── ProgressEvent ─────────────────────────────────────────────────────────────
// Sent to the frontend via SSE during a benchmark run.
struct ProgressEvent {
    std::string type;       // "started" | "progress" | "done" | "error"
    int         run_id;
    int         model_id;
    std::string model_name;
    int         current;    // current model index (1-based)
    int         total;      // total models in this session
    std::string message;    // human-readable status line
    double      progress;   // 0.0 - 1.0
};

// ── ProgressCallback ──────────────────────────────────────────────────────────
// Called by the runner on the benchmark thread.
// The server layer converts these into SSE messages.
using ProgressCallback = std::function<void(const ProgressEvent&)>;

// ── Runner ────────────────────────────────────────────────────────────────────
class Runner {
public:
    Runner(const Config& config, Crud& crud);

    // Start a benchmark session. Blocks until all models are done.
    // Calls progress_cb on each state change.
    // Returns list of run_ids created.
    std::vector<int> run(const BenchmarkRequest& request,
                         ProgressCallback        progress_cb);

    // Returns true if a benchmark session is currently running.
    bool is_running() const;

    // Abort the current session (sets a flag, current subprocess finishes).
    void abort();

private:
    const Config&    config_;
    Crud&            crud_;
    std::atomic_bool running_;
    std::atomic_bool abort_requested_;
    mutable std::mutex mutex_;

    // Run benchmark for a single model. Returns the run_id.
    int run_single(const Model&          model,
                   const BenchmarkConfig& cfg,
                   int                   current,
                   int                   total,
                   ProgressCallback&     progress_cb);

    // Build the llama-bench command line arguments.
    std::vector<std::string> build_args(const Model&           model,
                                        const BenchmarkConfig& cfg) const;

    // Launch llama-bench as a subprocess and capture stdout.
    // Returns the full stdout string. Throws on non-zero exit.
    std::string launch_process(const std::vector<std::string>& args) const;

    // Emit a progress event.
    void emit(ProgressCallback&    cb,
              const std::string&   type,
              int                  run_id,
              const Model&         model,
              int                  current,
              int                  total,
              const std::string&   message,
              double               progress) const;
};

} // namespace benchforge