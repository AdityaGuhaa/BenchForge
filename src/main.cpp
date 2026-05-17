// ─────────────────────────────────────────────────────────────────────────────
// BenchForge — entry point
//
// Wires up Config → Database → Crud → Scanner → Runner → Exporter → Server
// and starts the HTTP server. All long-lived services are stack-allocated in
// main() so destruction order is well-defined: Server stops first, then the
// services it depends on, and the SQLite connection closes last.
// ─────────────────────────────────────────────────────────────────────────────

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#include "config/config.hpp"
#include "db/database.hpp"
#include "db/crud.hpp"
#include "discovery/scanner.hpp"
#include "benchmark/runner.hpp"
#include "export/exporter.hpp"
#include "server/server.hpp"
#include "utils/system_info.hpp"

namespace fs = std::filesystem;

// ── Signal handling ─────────────────────────────────────────────────────────
// We keep a process-global pointer to the server so the signal handler can
// ask it to stop. This is the simplest portable pattern -- the alternative
// (sigaction with siginfo_t) doesn't buy us anything here.
namespace {
benchforge::Server*    g_server      = nullptr;
std::atomic_bool       g_shutdown{false};

extern "C" void on_signal(int /*sig*/) {
    if (g_shutdown.exchange(true)) {
        // Second Ctrl+C: bail out hard.
        std::_Exit(130);
    }
    std::cerr << "\n[BenchForge] Shutdown requested. Stopping server...\n";
    if (g_server) g_server->stop();
}
} // anonymous namespace

// ── CLI ─────────────────────────────────────────────────────────────────────
struct CliArgs {
    std::string config_path = "config.toml";
    int         port_override = -1;          // -1 = use config value
    bool        no_browser    = false;
    bool        show_help     = false;
};

static void print_usage() {
    std::cout <<
        "BenchForge -- local LLM benchmarking workbench\n\n"
        "Usage: BenchForge [options]\n\n"
        "Options:\n"
        "  -c, --config <path>   Path to config.toml (default: ./config.toml)\n"
        "  -p, --port <n>        Override server port from config\n"
        "      --no-browser      Don't auto-open the browser on start\n"
        "  -h, --help            Show this help and exit\n";
}

static CliArgs parse_args(int argc, char** argv) {
    CliArgs args;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* flag) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error(std::string("Missing value for ") + flag);
            }
            return argv[++i];
        };

        if      (a == "-h" || a == "--help")       args.show_help = true;
        else if (a == "-c" || a == "--config")     args.config_path = next("--config");
        else if (a == "-p" || a == "--port")       args.port_override = std::stoi(next("--port"));
        else if (a == "--no-browser")              args.no_browser = true;
        else {
            throw std::runtime_error("Unknown argument: " + a);
        }
    }
    return args;
}

// ── Banner ──────────────────────────────────────────────────────────────────
static void print_banner(const benchforge::Config& cfg) {
    using benchforge::SystemInfo;

    std::cout <<
        "\n"
        "  ╔══════════════════════════════════════╗\n"
        "  ║           B e n c h F o r g e        ║\n"
        "  ║  local LLM benchmarking workbench    ║\n"
        "  ╚══════════════════════════════════════╝\n\n";

    std::cout << "  CPU         : " << SystemInfo::cpu_name()
              << "  (" << SystemInfo::cpu_core_count() << " cores)\n";
    std::cout << "  GPU         : " << SystemInfo::gpu_name()
              << (SystemInfo::nvml_available() ? "  [NVML]" : "  [no NVML]") << "\n";
    std::cout << "  DB          : " << cfg.db_path << "\n";
    std::cout << "  Frontend    : http://localhost:" << cfg.port << "\n";
    std::cout << "  llama-bench : " << cfg.llama_bench_path << "\n";
    std::cout << "  Scan dirs   : ";
    if (cfg.scan_dirs.empty()) {
        std::cout << "(none configured)\n";
    } else {
        for (size_t i = 0; i < cfg.scan_dirs.size(); ++i) {
            if (i > 0) std::cout << "                ";
            std::cout << cfg.scan_dirs[i] << "\n";
        }
    }
    std::cout << "\n";
}

// ── main ────────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    using namespace benchforge;

    // 1. Parse CLI
    CliArgs args;
    try {
        args = parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "[BenchForge] Argument error: " << e.what() << "\n\n";
        print_usage();
        return 2;
    }
    if (args.show_help) {
        print_usage();
        return 0;
    }

    // 2. Load config (fall back to defaults if file is missing)
    Config cfg;
    try {
        if (fs::exists(args.config_path)) {
            cfg = load_config(args.config_path);
            std::cout << "[BenchForge] Loaded config from " << args.config_path << "\n";
        } else {
            std::cerr << "[BenchForge] Config not found at " << args.config_path
                      << " -- using built-in defaults.\n";
            cfg = default_config();
        }
    } catch (const std::exception& e) {
        std::cerr << "[BenchForge] Failed to load config: " << e.what() << "\n";
        return 1;
    }

    // Apply CLI overrides
    if (args.port_override > 0) cfg.port         = args.port_override;
    if (args.no_browser)        cfg.open_browser = false;

    print_banner(cfg);

    // 3. Wire up services. Stack-allocated in dependency order.
    try {
        Database db(cfg.db_path);
        Crud     crud(db);
        Scanner  scanner(cfg, crud);
        Runner   runner(cfg, crud);
        Exporter exporter(cfg);
        Server   server(cfg, db, crud, scanner, runner, exporter);

        // Make server reachable from the signal handler.
        g_server = &server;
        std::signal(SIGINT,  on_signal);
        std::signal(SIGTERM, on_signal);

        // 4. Initial scan so the UI has something to show on first load.
        try {
            auto result = scanner.scan();
            std::cout << "[BenchForge] Initial scan: "
                      << result.new_models_found << " new, "
                      << result.models_deactivated << " deactivated, "
                      << result.total_active_models << " active.\n";
            for (const auto& err : result.errors) {
                std::cerr << "  scan warning: " << err << "\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "[BenchForge] Initial scan failed: " << e.what() << "\n";
            // Non-fatal -- the user can fix scan_dirs and rescan from the UI.
        }

        // 5. Start serving (blocks until stop() is called).
        server.start();

        g_server = nullptr;
        std::cout << "[BenchForge] Server stopped cleanly.\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "[BenchForge] Fatal error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "[BenchForge] Fatal: unknown exception\n";
        return 1;
    }
}
