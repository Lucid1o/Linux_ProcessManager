#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace pulse::core {

enum class ProcessState : char {
    Running = 'R',
    Sleeping = 'S',
    DiskSleep = 'D',
    Zombie = 'Z',
    Stopped = 'T',
    TracingStop = 't',
    Dead = 'X',
    Wakekill = 'K',
    Waking = 'W',
    Parked = 'P',
    Idle = 'I',
    Unknown = '?'
};

inline const char* process_state_to_string(ProcessState state) {
    switch (state) {
        case ProcessState::Running: return "Running";
        case ProcessState::Sleeping: return "Sleeping";
        case ProcessState::DiskSleep: return "Disk Sleep (Uninterruptible)";
        case ProcessState::Zombie: return "Zombie (Defunct)";
        case ProcessState::Stopped: return "Stopped";
        case ProcessState::TracingStop: return "Tracing Stop";
        case ProcessState::Dead: return "Dead";
        case ProcessState::Wakekill: return "Wakekill";
        case ProcessState::Waking: return "Waking";
        case ProcessState::Parked: return "Parked";
        case ProcessState::Idle: return "Idle";
        default: return "Unknown";
    }
}

enum class ProcessSignal {
    Term = 15,
    Kill = 9,
    Stop = 19,
    Cont = 18
};

struct ProcessInfo {
    int pid{0};
    int ppid{0};
    std::string name;             // comm name
    std::string cmdline;          // full command line
    std::string exe_path;         // resolved target executable path
    std::string user;             // username owner
    int uid{0};
    ProcessState state{ProcessState::Unknown};
    int priority{0};
    int nice{0};
    int num_threads{1};
    double cpu_percent{0.0};
    uint64_t memory_rss_bytes{0};
    uint64_t memory_vsz_bytes{0};
    uint64_t memory_swap_bytes{0};
    uint64_t start_time_ticks{0}; // ticks after boot, used to detect PID reuse
    std::string cgroup;           // /proc/<pid>/cgroup content
    bool is_kernel_thread{false}; // PID 2 or PPID 2 / kthread
};

struct ProcessNode {
    ProcessInfo info;
    std::vector<std::shared_ptr<ProcessNode>> children;
    std::weak_ptr<ProcessNode> parent;
};

struct ApplicationGroup {
    std::string app_id;           // Unique identifier (e.g., "google-chrome", "code", "spotify")
    std::string display_name;     // Friendly name (e.g., "Google Chrome", "Visual Studio Code")
    std::string icon_name;        // Freedesktop icon name
    int root_pid{0};              // App root process PID
    std::vector<int> process_pids;// List of all PIDs belonging to this app
    double total_cpu_percent{0.0};
    uint64_t total_memory_rss_bytes{0};
    uint64_t total_memory_swap_bytes{0};
    int total_threads{0};
    int process_count{0};
    std::vector<std::shared_ptr<ProcessNode>> root_process_trees;
};

struct CpuCoreMetrics {
    int core_id{0};
    double usage_percent{0.0};
};

struct DiskMetrics {
    uint64_t read_bytes_per_sec{0};
    uint64_t write_bytes_per_sec{0};
    uint64_t total_read_bytes{0};
    uint64_t total_write_bytes{0};
};

struct NetworkMetrics {
    uint64_t rx_bytes_per_sec{0};
    uint64_t tx_bytes_per_sec{0};
    uint64_t total_rx_bytes{0};
    uint64_t total_tx_bytes{0};
};

struct SystemMetrics {
    double total_cpu_percent{0.0};
    std::vector<CpuCoreMetrics> cores;
    uint64_t mem_total_bytes{0};
    uint64_t mem_available_bytes{0};
    uint64_t mem_used_bytes{0};
    uint64_t swap_total_bytes{0};
    uint64_t swap_free_bytes{0};
    uint64_t swap_used_bytes{0};
    DiskMetrics disk;
    NetworkMetrics network;
    int total_processes{0};
    int total_threads{0};
};

} // namespace pulse::core
