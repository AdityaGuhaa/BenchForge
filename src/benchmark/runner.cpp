#include "runner.hpp"

#include <stdexcept>
#include <sstream>
#include <chrono>
#include <thread>
#include <filesystem>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

namespace benchforge {

Runner::Runner(const Config& config, Crud& crud)
    : config_(config)
    , crud_(crud)
    , running_(false)
    , abort_requested_(false)
{}

// ── Public ────────────────────────────────────────────────────────────────────

bool Runner::is_running() const {
    return running_.load();
}

void Runner::abort() {
    abort_requested_ = true;
}

std::vector<int> Runner::run(const BenchmarkRequest& request,
                              ProgressCallback        progress_cb) {
    if (running_.exchange(true)) {
        throw std::runtime_error("A benchmark session is already running");
    }

    abort_requested_ = false;
    std::vector<int> run_ids;
    int total = (int)request.model_ids.size();

    try {
        int current = 1;
        for (int model_id : request.model_ids) {
            if (abort_requested_) break;

            auto model_opt = crud_.get_model_by_id(model_id);
            if (!model_opt.has_value()) {
                // Skip invalid model ids silently
                current++;
                continue;
            }

            int run_id = run_single(
                *model_opt, request.config,
                current, total, progress_cb
            );
            run_ids.push_back(run_id);
            current++;
        }
    } catch (...) {
        running_ = false;
        throw;
    }

    running_ = false;
    return run_ids;
}

// ── Private ───────────────────────────────────────────────────────────────────

int Runner::run_single(const Model&           model,
                        const BenchmarkConfig& cfg,
                        int                    current,
                        int                    total,
                        ProgressCallback&      progress_cb) {
    // Create run record
    int run_id = crud_.insert_run(model.id, cfg.id);

    emit(progress_cb, "started", run_id, model, current, total,
         "Starting benchmark for " + model.name, 0.0);

    crud_.mark_run_started(run_id);

    // Start memory poller
    MemoryPoller poller(250);
    poller.start();

    std::string raw_output;
    bool        failed = false;
    std::string error_msg;

    try {
        emit(progress_cb, "progress", run_id, model, current, total,
             "Running llama-bench...", 0.2);

        auto args   = build_args(model, cfg);
        raw_output  = launch_process(args);

        emit(progress_cb, "progress", run_id, model, current, total,
             "Parsing results...", 0.8);

    } catch (const std::exception& e) {
        failed    = true;
        error_msg = e.what();
    }

    // Stop memory poller before writing results
    poller.stop();

    if (failed) {
        crud_.mark_run_finished(run_id);
        crud_.update_run_status(run_id, "failed", error_msg);

        emit(progress_cb, "error", run_id, model, current, total,
             "Failed: " + error_msg, 1.0);

        return run_id;
    }

    // Parse metrics
    try {
        ParsedMetrics metrics = MetricsParser::parse(raw_output);

        BenchmarkResult result{};
        result.run_id                 = run_id;
        result.tokens_per_second      = metrics.tokens_per_second;
        result.prompt_tokens_per_sec  = metrics.prompt_tokens_per_sec;
        result.gen_tokens_per_sec     = metrics.gen_tokens_per_sec;
        result.time_to_first_token_ms = metrics.time_to_first_token_ms;
        result.ram_usage_mb           = poller.peak_ram_mb();
        result.vram_usage_mb          = poller.peak_vram_mb();
        result.perplexity             = -1.0; // populated by perplexity.cpp if opted in
        result.raw_output             = raw_output;

        crud_.insert_result(result);
        crud_.mark_run_finished(run_id);
        crud_.update_run_status(run_id, "done");

        emit(progress_cb, "done", run_id, model, current, total,
             "Done. " + std::to_string((int)metrics.tokens_per_second) + " tok/s",
             1.0);

    } catch (const std::exception& e) {
        crud_.mark_run_finished(run_id);
        crud_.update_run_status(run_id, "failed", e.what());

        emit(progress_cb, "error", run_id, model, current, total,
             "Parse failed: " + std::string(e.what()), 1.0);
    }

    return run_id;
}

std::vector<std::string> Runner::build_args(const Model&           model,
                                             const BenchmarkConfig& cfg) const {
    std::vector<std::string> args;

    args.push_back(config_.llama_bench_path);

    // Model path
    args.push_back("-m");
    args.push_back(model.path);

    // Prompt tokens
    args.push_back("-p");
    args.push_back(std::to_string(cfg.prompt_tokens));

    // Generation tokens
    args.push_back("-n");
    args.push_back(std::to_string(cfg.generation_tokens));

    // Repetitions
    args.push_back("-r");
    args.push_back(std::to_string(cfg.repetitions));

    // Threads
    args.push_back("-t");
    args.push_back(std::to_string(cfg.threads));

    // GPU layers
    args.push_back("-ngl");
    args.push_back(std::to_string(cfg.gpu_layers));

    // Output format -- critical for our JSON parser
    args.push_back("-o");
    args.push_back("json");

    return args;
}

std::string Runner::launch_process(const std::vector<std::string>& args) const {
#ifdef _WIN32
    // Build command string
    std::ostringstream cmd;
    for (size_t i = 0; i < args.size(); i++) {
        if (i > 0) cmd << " ";
        // Wrap in quotes to handle paths with spaces
        cmd << "\"" << args[i] << "\"";
    }
    std::string cmd_str = cmd.str();

    // Set up pipes for stdout capture
    SECURITY_ATTRIBUTES sa{};
    sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle       = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE stdout_read  = nullptr;
    HANDLE stdout_write = nullptr;

    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        throw std::runtime_error("Failed to create stdout pipe");
    }
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb         = sizeof(STARTUPINFOA);
    si.hStdOutput = stdout_write;
    si.hStdError  = stdout_write; // capture stderr too
    si.dwFlags   |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi{};

    BOOL created = CreateProcessA(
        nullptr,
        const_cast<char*>(cmd_str.c_str()),
        nullptr, nullptr,
        TRUE,           // inherit handles
        0, nullptr, nullptr,
        &si, &pi
    );

    CloseHandle(stdout_write); // parent doesn't write

    if (!created) {
        CloseHandle(stdout_read);
        throw std::runtime_error(
            "Failed to launch llama-bench. Check llama_bench_path in config.toml"
        );
    }

    // Read stdout until process exits
    std::string output;
    char buf[4096];
    DWORD bytes_read = 0;

    while (ReadFile(stdout_read, buf, sizeof(buf) - 1, &bytes_read, nullptr)
           && bytes_read > 0) {
        buf[bytes_read] = '\0';
        output += buf;
    }

    CloseHandle(stdout_read);

    // Wait for process to finish
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exit_code != 0) {
        throw std::runtime_error(
            "llama-bench exited with code " + std::to_string(exit_code) +
            "\nOutput: " + output.substr(0, 500)
        );
    }

    return output;

#else
    // POSIX fallback (Linux/macOS -- for future platform support)
    std::ostringstream cmd;
    for (size_t i = 0; i < args.size(); i++) {
        if (i > 0) cmd << " ";
        cmd << "\"" << args[i] << "\"";
    }
    cmd << " 2>&1";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Failed to launch llama-bench via popen");
    }

    std::string output;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe))
        output += buf;

    int exit_code = pclose(pipe);
    if (exit_code != 0) {
        throw std::runtime_error(
            "llama-bench exited with code " + std::to_string(exit_code)
        );
    }

    return output;
#endif
}

void Runner::emit(ProgressCallback&    cb,
                   const std::string&   type,
                   int                  run_id,
                   const Model&         model,
                   int                  current,
                   int                  total,
                   const std::string&   message,
                   double               progress) const {
    if (!cb) return;
    ProgressEvent ev{};
    ev.type       = type;
    ev.run_id     = run_id;
    ev.model_id   = model.id;
    ev.model_name = model.name;
    ev.current    = current;
    ev.total      = total;
    ev.message    = message;
    ev.progress   = progress;
    cb(ev);
}

} // namespace benchforge