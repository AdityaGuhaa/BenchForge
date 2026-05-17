#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>

namespace benchforge {

// ── SystemSnapshot ────────────────────────────────────────────────────────────
// A single point-in-time reading of system memory.
struct SystemSnapshot {
    double ram_used_mb;    // current process RAM usage in MB
    double ram_total_mb;   // total system RAM in MB
    double vram_used_mb;   // GPU VRAM used in MB (-1.0 if unavailable)
    double vram_total_mb;  // GPU VRAM total in MB (-1.0 if unavailable)
    bool   gpu_available;  // true if NVML initialized successfully
};

// ── SystemInfo ────────────────────────────────────────────────────────────────
// Static helpers for one-shot system queries.
struct SystemInfo {
    // Returns current memory snapshot.
    static SystemSnapshot snapshot();

    // Returns total logical CPU cores.
    static int cpu_core_count();

    // Returns CPU name string (e.g. "Intel Core i9-13900H").
    static std::string cpu_name();

    // Returns GPU name string (e.g. "NVIDIA GeForce RTX 4050").
    // Returns "N/A" if no GPU or NVML unavailable.
    static std::string gpu_name();

    // Returns true if NVML is available and initialized.
    static bool nvml_available();
};

// ── MemoryPoller ──────────────────────────────────────────────────────────────
// Polls RAM and VRAM usage on a background thread during a benchmark run.
// Call start() before the run, stop() after. peak_ram_mb() and peak_vram_mb()
// return the maximums observed during the polling window.
class MemoryPoller {
public:
    // poll_interval_ms: how often to sample (default 250ms)
    explicit MemoryPoller(int poll_interval_ms = 250);
    ~MemoryPoller();

    // Non-copyable
    MemoryPoller(const MemoryPoller&)            = delete;
    MemoryPoller& operator=(const MemoryPoller&) = delete;

    // Start polling on a background thread.
    void start();

    // Stop polling and join the background thread.
    void stop();

    // Returns peak RAM usage observed since start() in MB.
    double peak_ram_mb() const;

    // Returns peak VRAM usage observed since start() in MB.
    // Returns -1.0 if GPU monitoring is unavailable.
    double peak_vram_mb() const;

    // Reset peak values (call before reuse).
    void reset();

private:
    int              poll_interval_ms_;
    std::thread      thread_;
    std::atomic_bool running_;
    mutable std::mutex mutex_;

    double peak_ram_mb_;
    double peak_vram_mb_;

    void poll_loop();
};

} // namespace benchforge