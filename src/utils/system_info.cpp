#include "system_info.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <chrono>

// Windows-specific headers
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <psapi.h>
    #pragma comment(lib, "psapi.lib")
#endif

// NVML (optional -- only compiled if BENCHFORGE_NVML_ENABLED is defined)
#ifdef BENCHFORGE_NVML_ENABLED
    #include <nvml.h>
    #pragma comment(lib, "nvml.lib")
#endif

namespace benchforge {

// ── NVML Init (internal) ──────────────────────────────────────────────────────

namespace {

bool nvml_init_attempted = false;
bool nvml_initialized    = false;

void ensure_nvml_init() {
#ifdef BENCHFORGE_NVML_ENABLED
    if (nvml_init_attempted) return;
    nvml_init_attempted = true;
    nvml_initialized = (nvmlInit() == NVML_SUCCESS);
#endif
}

} // anonymous namespace

// ── SystemInfo ────────────────────────────────────────────────────────────────

bool SystemInfo::nvml_available() {
    ensure_nvml_init();
    return nvml_initialized;
}

SystemSnapshot SystemInfo::snapshot() {
    SystemSnapshot snap{};
    snap.gpu_available = false;
    snap.vram_used_mb  = -1.0;
    snap.vram_total_mb = -1.0;

#ifdef _WIN32
    // RAM: current process working set
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        snap.ram_used_mb = (double)pmc.WorkingSetSize / (1024.0 * 1024.0);
    }

    // Total system RAM
    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);
    if (GlobalMemoryStatusEx(&mem_status)) {
        snap.ram_total_mb = (double)mem_status.ullTotalPhys / (1024.0 * 1024.0);
    }
#endif

#ifdef BENCHFORGE_NVML_ENABLED
    ensure_nvml_init();
    if (nvml_initialized) {
        nvmlDevice_t device;
        if (nvmlDeviceGetHandleByIndex(0, &device) == NVML_SUCCESS) {
            nvmlMemory_t mem_info;
            if (nvmlDeviceGetMemoryInfo(device, &mem_info) == NVML_SUCCESS) {
                snap.vram_used_mb  = (double)mem_info.used  / (1024.0 * 1024.0);
                snap.vram_total_mb = (double)mem_info.total / (1024.0 * 1024.0);
                snap.gpu_available = true;
            }
        }
    }
#endif

    return snap;
}

int SystemInfo::cpu_core_count() {
#ifdef _WIN32
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    return (int)sys_info.dwNumberOfProcessors;
#else
    return (int)std::thread::hardware_concurrency();
#endif
}

std::string SystemInfo::cpu_name() {
#ifdef _WIN32
    // Read from registry
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        char buf[256] = {};
        DWORD buf_size = sizeof(buf);
        RegQueryValueExA(hKey, "ProcessorNameString",
            nullptr, nullptr, (LPBYTE)buf, &buf_size);
        RegCloseKey(hKey);
        return std::string(buf);
    }
#endif
    return "Unknown CPU";
}

std::string SystemInfo::gpu_name() {
#ifdef BENCHFORGE_NVML_ENABLED
    ensure_nvml_init();
    if (nvml_initialized) {
        nvmlDevice_t device;
        if (nvmlDeviceGetHandleByIndex(0, &device) == NVML_SUCCESS) {
            char name[96] = {};
            if (nvmlDeviceGetName(device, name, sizeof(name)) == NVML_SUCCESS)
                return std::string(name);
        }
    }
#endif
    return "N/A";
}

// ── MemoryPoller ──────────────────────────────────────────────────────────────

MemoryPoller::MemoryPoller(int poll_interval_ms)
    : poll_interval_ms_(poll_interval_ms)
    , running_(false)
    , peak_ram_mb_(0.0)
    , peak_vram_mb_(-1.0)
{}

MemoryPoller::~MemoryPoller() {
    if (running_) stop();
}

void MemoryPoller::start() {
    running_ = true;
    thread_  = std::thread(&MemoryPoller::poll_loop, this);
}

void MemoryPoller::stop() {
    running_ = false;
    if (thread_.joinable())
        thread_.join();
}

void MemoryPoller::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    peak_ram_mb_  = 0.0;
    peak_vram_mb_ = -1.0;
}

double MemoryPoller::peak_ram_mb() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return peak_ram_mb_;
}

double MemoryPoller::peak_vram_mb() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return peak_vram_mb_;
}

void MemoryPoller::poll_loop() {
    while (running_) {
        auto snap = SystemInfo::snapshot();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            peak_ram_mb_ = std::max(peak_ram_mb_, snap.ram_used_mb);

            if (snap.gpu_available) {
                peak_vram_mb_ = std::max(
                    peak_vram_mb_ < 0.0 ? 0.0 : peak_vram_mb_,
                    snap.vram_used_mb
                );
            }
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(poll_interval_ms_)
        );
    }
}

} // namespace benchforge