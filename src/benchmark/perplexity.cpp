#include "perplexity.hpp"

#include <stdexcept>
#include <sstream>
#include <regex>
#include <filesystem>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

namespace benchforge {

PerplexityRunner::PerplexityRunner(const Config& config)
    : config_(config)
{}

// ── Public ────────────────────────────────────────────────────────────────────

PerplexityResult PerplexityRunner::run(
    const Model&                             model,
    std::function<void(const std::string&)>  progress_cb)
{
    PerplexityResult result{};
    result.success = false;

    // Validate reference file exists
    if (!std::filesystem::exists(config_.reference_file)) {
        result.error = "Reference file not found: " + config_.reference_file +
                       "\nPlace a plain text file at this path (e.g. wiki.test.txt)";
        return result;
    }

    // Validate llama-perplexity binary exists
    if (!std::filesystem::exists(config_.llama_perplexity_path)) {
        result.error = "llama-perplexity not found at: " +
                       config_.llama_perplexity_path +
                       "\nUpdate llama_perplexity_path in config.toml";
        return result;
    }

    if (progress_cb)
        progress_cb("Launching llama-perplexity for " + model.name + "...");

    try {
        auto args      = build_args(model);
        auto raw_out   = launch_process(args);
        result.raw_output = raw_out;

        if (progress_cb)
            progress_cb("Parsing perplexity score...");

        auto [score, uncertainty] = parse_output(raw_out);
        result.score       = score;
        result.uncertainty = uncertainty;
        result.success     = true;

    } catch (const std::exception& e) {
        result.error = e.what();
    }

    return result;
}

// ── Private ───────────────────────────────────────────────────────────────────

std::vector<std::string> PerplexityRunner::build_args(const Model& model) const {
    std::vector<std::string> args;

    args.push_back(config_.llama_perplexity_path);

    // Model path
    args.push_back("-m");
    args.push_back(model.path);

    // Reference text file
    args.push_back("-f");
    args.push_back(config_.reference_file);

    // Threads
    args.push_back("-t");
    args.push_back(std::to_string(config_.threads));

    // GPU layers
    args.push_back("-ngl");
    args.push_back(std::to_string(config_.gpu_layers));

    return args;
}

std::string PerplexityRunner::launch_process(
    const std::vector<std::string>& args) const
{
#ifdef _WIN32
    // Build quoted command string
    std::ostringstream cmd;
    for (size_t i = 0; i < args.size(); i++) {
        if (i > 0) cmd << " ";
        cmd << "\"" << args[i] << "\"";
    }
    std::string cmd_str = cmd.str();

    SECURITY_ATTRIBUTES sa{};
    sa.nLength        = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    HANDLE stdout_read  = nullptr;
    HANDLE stdout_write = nullptr;

    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0))
        throw std::runtime_error("Failed to create stdout pipe");

    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb         = sizeof(STARTUPINFOA);
    si.hStdOutput = stdout_write;
    si.hStdError  = stdout_write;
    si.dwFlags   |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi{};

    BOOL created = CreateProcessA(
        nullptr,
        const_cast<char*>(cmd_str.c_str()),
        nullptr, nullptr,
        TRUE, 0, nullptr, nullptr,
        &si, &pi
    );

    CloseHandle(stdout_write);

    if (!created) {
        CloseHandle(stdout_read);
        throw std::runtime_error(
            "Failed to launch llama-perplexity. "
            "Check llama_perplexity_path in config.toml"
        );
    }

    // Read output
    std::string output;
    char        buf[4096];
    DWORD       bytes_read = 0;

    while (ReadFile(stdout_read, buf, sizeof(buf) - 1, &bytes_read, nullptr)
           && bytes_read > 0) {
        buf[bytes_read] = '\0';
        output += buf;
    }

    CloseHandle(stdout_read);
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exit_code != 0) {
        throw std::runtime_error(
            "llama-perplexity exited with code " +
            std::to_string(exit_code) +
            "\nOutput: " + output.substr(0, 500)
        );
    }

    return output;

#else
    // POSIX fallback
    std::ostringstream cmd;
    for (size_t i = 0; i < args.size(); i++) {
        if (i > 0) cmd << " ";
        cmd << "\"" << args[i] << "\"";
    }
    cmd << " 2>&1";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe)
        throw std::runtime_error("Failed to launch llama-perplexity");

    std::string output;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe))
        output += buf;

    int exit_code = pclose(pipe);
    if (exit_code != 0) {
        throw std::runtime_error(
            "llama-perplexity exited with code " +
            std::to_string(exit_code)
        );
    }

    return output;
#endif
}

std::pair<double, double> PerplexityRunner::parse_output(
    const std::string& output) const
{
    // llama-perplexity outputs a line like:
    // "Perplexity: 6.8341 +/- 0.0412"
    // We use a regex to extract both numbers.
    std::regex  ppl_regex(
        R"([Pp]erplexity[^\d]*(\d+(?:\.\d+)?)\s*\+/-\s*(\d+(?:\.\d+)?))"
    );
    std::smatch match;

    if (std::regex_search(output, match, ppl_regex)) {
        double score       = std::stod(match[1].str());
        double uncertainty = std::stod(match[2].str());
        return { score, uncertainty };
    }

    // Fallback: try matching just the score without uncertainty
    std::regex simple_regex(R"([Pp]erplexity[^\d]*(\d+(?:\.\d+)?))");
    if (std::regex_search(output, match, simple_regex)) {
        return { std::stod(match[1].str()), 0.0 };
    }

    throw std::runtime_error(
        "Could not find perplexity score in llama-perplexity output.\n"
        "Raw output (first 300 chars): " + output.substr(0, 300)
    );
}

} // namespace benchforge