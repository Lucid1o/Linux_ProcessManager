/**
 * LinuxProcCollector.hpp - Linux /proc and /sys collector for Pulse
 *
 * Attribution notice:
 * Adapted and inspired by the Linux collection methodology of btop++
 * (https://github.com/aristocratos/btop, Apache License 2.0).
 * btop++ is Copyright 2021 Jakob P. Liljenberg (aristocratos).
 *
 * Specific btop++ techniques adapted here:
 * 1. CPU utilization calculation using delta of total and idle jiffies from /proc/stat.
 * 2. Process CPU percentage calculation using (utime + stime) delta divided by total CPU jiffies.
 * 3. MemAvailable / MemTotal parsing from /proc/meminfo.
 * 4. Sector-based I/O rate calculation from /proc/diskstats.
 * 5. Interface byte throughput rate parsing from /proc/net/dev.
 */

#pragma once

#include "pulse/collector/system_collector.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

namespace pulse::collector {

class LinuxProcCollector : public ISystemCollector {
public:
    explicit LinuxProcCollector(
        std::string proc_root = "/proc",
        std::string sys_root = "/sys",
        long clk_tck = 100);

    void update() override;
    const SystemMetrics& get_system_metrics() const override { return metrics_; }
    const std::vector<ProcessInfo>& get_processes() const override { return processes_; }
    std::optional<ProcessInfo> get_process_by_pid(int pid) const override;

    const std::string& proc_root() const { return proc_root_; }
    const std::string& sys_root() const { return sys_root_; }

private:
    std::string proc_root_;
    std::string sys_root_;
    long clk_tck_{100};

    SystemMetrics metrics_;
    std::vector<ProcessInfo> processes_;
    std::unordered_map<int, ProcessInfo> proc_map_;

    // Previous CPU state for delta calculations (btop++ approach)
    struct CpuTimes {
        uint64_t total{0};
        uint64_t idle{0};
    };
    CpuTimes prev_total_cpu_;
    std::vector<CpuTimes> prev_cores_cpu_;

    // Previous process CPU ticks for process CPU % calculation (PID -> (utime + stime))
    std::unordered_map<int, uint64_t> prev_proc_ticks_;

    // Previous disk I/O state
    uint64_t prev_disk_read_bytes_{0};
    uint64_t prev_disk_write_bytes_{0};

    // Previous network state
    uint64_t prev_net_rx_bytes_{0};
    uint64_t prev_net_tx_bytes_{0};

    std::chrono::steady_clock::time_point prev_time_;

    void collect_cpu(double elapsed_sec);
    void collect_memory();
    void collect_disk(double elapsed_sec);
    void collect_network(double elapsed_sec);
    void collect_processes(uint64_t total_cpu_jiffies_delta);

    // Helpers
    std::string read_file_contents(const std::string& path) const;
};

} // namespace pulse::collector
