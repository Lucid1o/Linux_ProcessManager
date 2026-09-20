/**
 * linux_proc_collector.cpp - Linux /proc and /sys collector implementation
 *
 * Attribution notice:
 * Adapted and inspired by the Linux collection methodology of btop++
 * (https://github.com/aristocratos/btop, Apache License 2.0).
 * btop++ is Copyright 2021 Jakob P. Liljenberg (aristocratos).
 */

#include "pulse/collector/linux_proc_collector.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace pulse::collector {

using pulse::core::ProcessState;

LinuxProcCollector::LinuxProcCollector(std::string proc_root, std::string sys_root, long clk_tck)
    : proc_root_(std::move(proc_root))
    , sys_root_(std::move(sys_root))
    , clk_tck_(clk_tck > 0 ? clk_tck : 100)
    , prev_time_(std::chrono::steady_clock::now()) {
}

std::string LinuxProcCollector::read_file_contents(const std::string& path) const {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::optional<ProcessInfo> LinuxProcCollector::get_process_by_pid(int pid) const {
    auto it = proc_map_.find(pid);
    if (it != proc_map_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void LinuxProcCollector::update() {
    auto now = std::chrono::steady_clock::now();
    double elapsed_sec = std::chrono::duration_cast<std::chrono::duration<double>>(now - prev_time_).count();
    if (elapsed_sec <= 0.0) elapsed_sec = 1.0;
    prev_time_ = now;

    // 1. CPU
    collect_cpu(elapsed_sec);

    // 2. Memory & Swap
    collect_memory();

    // 3. Disk I/O
    collect_disk(elapsed_sec);

    // 4. Network throughput
    collect_network(elapsed_sec);

    // Calculate total system CPU jiffies delta for process calculation
    uint64_t total_jiffies_delta = 0;
    if (prev_total_cpu_.total > 0) {
        // Will be used in collect_processes
    }

    // 5. Processes
    collect_processes(total_jiffies_delta);

    metrics_.total_processes = static_cast<int>(processes_.size());
    int total_threads = 0;
    for (const auto& p : processes_) {
        total_threads += p.num_threads;
    }
    metrics_.total_threads = total_threads;
}

void LinuxProcCollector::collect_cpu(double elapsed_sec) {
    (void)elapsed_sec;
    std::string stat_data = read_file_contents(proc_root_ + "/stat");
    if (stat_data.empty()) return;

    std::istringstream stream(stat_data);
    std::string line;

    bool first_line = true;
    std::vector<CpuTimes> current_cores;

    while (std::getline(stream, line)) {
        if (line.rfind("cpu", 0) != 0) break; // Reached non-cpu lines

        std::istringstream iss(line);
        std::string cpu_label;
        iss >> cpu_label;

        uint64_t user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;
        iss >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

        uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;
        uint64_t idle_all = idle + iowait;

        if (first_line) {
            // Aggregate CPU
            first_line = false;
            if (prev_total_cpu_.total > 0 && total >= prev_total_cpu_.total) {
                uint64_t d_total = total - prev_total_cpu_.total;
                uint64_t d_idle = (idle_all >= prev_total_cpu_.idle) ? (idle_all - prev_total_cpu_.idle) : 0;
                if (d_total > 0) {
                    metrics_.total_cpu_percent = std::clamp(100.0 * (1.0 - static_cast<double>(d_idle) / d_total), 0.0, 100.0);
                }
            }
            prev_total_cpu_ = {total, idle_all};
        } else {
            // Per-core CPU (e.g. cpu0, cpu1...)
            current_cores.push_back({total, idle_all});
        }
    }

    if (metrics_.cores.size() != current_cores.size()) {
        metrics_.cores.resize(current_cores.size());
        for (size_t i = 0; i < current_cores.size(); ++i) {
            metrics_.cores[i].core_id = static_cast<int>(i);
        }
    }

    if (prev_cores_cpu_.size() == current_cores.size()) {
        for (size_t i = 0; i < current_cores.size(); ++i) {
            uint64_t d_total = (current_cores[i].total >= prev_cores_cpu_[i].total)
                ? (current_cores[i].total - prev_cores_cpu_[i].total) : 0;
            uint64_t d_idle = (current_cores[i].idle >= prev_cores_cpu_[i].idle)
                ? (current_cores[i].idle - prev_cores_cpu_[i].idle) : 0;
            if (d_total > 0) {
                metrics_.cores[i].usage_percent = std::clamp(100.0 * (1.0 - static_cast<double>(d_idle) / d_total), 0.0, 100.0);
            }
        }
    }
    prev_cores_cpu_ = current_cores;
}

void LinuxProcCollector::collect_memory() {
    std::string mem_data = read_file_contents(proc_root_ + "/meminfo");
    if (mem_data.empty()) return;

    std::istringstream stream(mem_data);
    std::string line;

    uint64_t mem_total_kb = 0;
    uint64_t mem_free_kb = 0;
    uint64_t mem_available_kb = 0;
    uint64_t buffers_kb = 0;
    uint64_t cached_kb = 0;
    uint64_t swap_total_kb = 0;
    uint64_t swap_free_kb = 0;
    bool has_available = false;

    while (std::getline(stream, line)) {
        std::istringstream iss(line);
        std::string key;
        uint64_t val = 0;
        iss >> key >> val;

        if (key == "MemTotal:") mem_total_kb = val;
        else if (key == "MemFree:") mem_free_kb = val;
        else if (key == "MemAvailable:") { mem_available_kb = val; has_available = true; }
        else if (key == "Buffers:") buffers_kb = val;
        else if (key == "Cached:") cached_kb = val;
        else if (key == "SwapTotal:") swap_total_kb = val;
        else if (key == "SwapFree:") swap_free_kb = val;
    }

    metrics_.mem_total_bytes = mem_total_kb * 1024;
    metrics_.swap_total_bytes = swap_total_kb * 1024;
    metrics_.swap_free_bytes = swap_free_kb * 1024;
    metrics_.swap_used_bytes = (swap_total_kb >= swap_free_kb) ? (swap_total_kb - swap_free_kb) * 1024 : 0;

    if (has_available) {
        metrics_.mem_available_bytes = mem_available_kb * 1024;
        metrics_.mem_used_bytes = (mem_total_kb >= mem_available_kb) ? (mem_total_kb - mem_available_kb) * 1024 : 0;
    } else {
        uint64_t free_plus_cache = mem_free_kb + buffers_kb + cached_kb;
        metrics_.mem_available_bytes = free_plus_cache * 1024;
        metrics_.mem_used_bytes = (mem_total_kb >= free_plus_cache) ? (mem_total_kb - free_plus_cache) * 1024 : 0;
    }
}

void LinuxProcCollector::collect_disk(double elapsed_sec) {
    std::string disk_data = read_file_contents(proc_root_ + "/diskstats");
    if (disk_data.empty()) return;

    std::istringstream stream(disk_data);
    std::string line;

    uint64_t total_sectors_read = 0;
    uint64_t total_sectors_written = 0;

    while (std::getline(stream, line)) {
        std::istringstream iss(line);
        int major = 0, minor = 0;
        std::string dev_name;
        uint64_t reads = 0, reads_merged = 0, sec_read = 0, read_ms = 0;
        uint64_t writes = 0, writes_merged = 0, sec_written = 0, write_ms = 0;

        if (iss >> major >> minor >> dev_name >> reads >> reads_merged >> sec_read >> read_ms
                >> writes >> writes_merged >> sec_written >> write_ms) {
            // Filter loop, ram, and dm devices
            if (dev_name.rfind("loop", 0) == 0 || dev_name.rfind("ram", 0) == 0) {
                continue;
            }
            total_sectors_read += sec_read;
            total_sectors_written += sec_written;
        }
    }

    uint64_t cur_read_bytes = total_sectors_read * 512;
    uint64_t cur_write_bytes = total_sectors_written * 512;

    metrics_.disk.total_read_bytes = cur_read_bytes;
    metrics_.disk.total_write_bytes = cur_write_bytes;

    if (prev_disk_read_bytes_ > 0 && cur_read_bytes >= prev_disk_read_bytes_ && elapsed_sec > 0.0) {
        metrics_.disk.read_bytes_per_sec = static_cast<uint64_t>(
            (cur_read_bytes - prev_disk_read_bytes_) / elapsed_sec);
    } else {
        metrics_.disk.read_bytes_per_sec = 0;
    }

    if (prev_disk_write_bytes_ > 0 && cur_write_bytes >= prev_disk_write_bytes_ && elapsed_sec > 0.0) {
        metrics_.disk.write_bytes_per_sec = static_cast<uint64_t>(
            (cur_write_bytes - prev_disk_write_bytes_) / elapsed_sec);
    } else {
        metrics_.disk.write_bytes_per_sec = 0;
    }

    prev_disk_read_bytes_ = cur_read_bytes;
    prev_disk_write_bytes_ = cur_write_bytes;
}

void LinuxProcCollector::collect_network(double elapsed_sec) {
    std::string net_data = read_file_contents(proc_root_ + "/net/dev");
    if (net_data.empty()) return;

    std::istringstream stream(net_data);
    std::string line;

    // Skip first two header lines
    std::getline(stream, line);
    std::getline(stream, line);

    uint64_t total_rx = 0;
    uint64_t total_tx = 0;

    while (std::getline(stream, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string iface = line.substr(0, colon);
        auto start = iface.find_first_not_of(" \t");
        if (start != std::string::npos) iface = iface.substr(start);

        // Filter out loopback
        if (iface == "lo") continue;

        std::istringstream iss(line.substr(colon + 1));
        uint64_t rx_bytes = 0, rx_packets = 0, rx_err = 0, rx_drop = 0, rx_fifo = 0, rx_frame = 0, rx_compressed = 0, rx_multicast = 0;
        uint64_t tx_bytes = 0;

        if (iss >> rx_bytes >> rx_packets >> rx_err >> rx_drop >> rx_fifo >> rx_frame >> rx_compressed >> rx_multicast >> tx_bytes) {
            total_rx += rx_bytes;
            total_tx += tx_bytes;
        }
    }

    metrics_.network.total_rx_bytes = total_rx;
    metrics_.network.total_tx_bytes = total_tx;

    if (prev_net_rx_bytes_ > 0 && total_rx >= prev_net_rx_bytes_ && elapsed_sec > 0.0) {
        metrics_.network.rx_bytes_per_sec = static_cast<uint64_t>(
            (total_rx - prev_net_rx_bytes_) / elapsed_sec);
    } else {
        metrics_.network.rx_bytes_per_sec = 0;
    }

    if (prev_net_tx_bytes_ > 0 && total_tx >= prev_net_tx_bytes_ && elapsed_sec > 0.0) {
        metrics_.network.tx_bytes_per_sec = static_cast<uint64_t>(
            (total_tx - prev_net_tx_bytes_) / elapsed_sec);
    } else {
        metrics_.network.tx_bytes_per_sec = 0;
    }

    prev_net_rx_bytes_ = total_rx;
    prev_net_tx_bytes_ = total_tx;
}

void LinuxProcCollector::collect_processes(uint64_t total_cpu_jiffies_delta) {
    (void)total_cpu_jiffies_delta;
    processes_.clear();
    proc_map_.clear();

    std::error_code ec;
    std::filesystem::path root_p(proc_root_);
    if (!std::filesystem::exists(root_p, ec) || !std::filesystem::is_directory(root_p, ec)) {
        return;
    }

    std::unordered_map<int, uint64_t> cur_proc_ticks;

    for (const auto& entry : std::filesystem::directory_iterator(root_p, ec)) {
        if (!entry.is_directory()) continue;

        std::string dir_name = entry.path().filename().string();
        if (dir_name.empty() || !std::isdigit(static_cast<unsigned char>(dir_name[0]))) {
            continue; // Not a PID folder
        }

        int pid = std::atoi(dir_name.c_str());
        if (pid <= 0) continue;

        std::string pid_path = entry.path().string();
        std::string stat_str = read_file_contents(pid_path + "/stat");
        if (stat_str.empty()) continue;

        // Parse /proc/[pid]/stat
        // Format: pid (comm) state ppid pgrp session tty_nr tpgid flags minflt cminflt majflt cmajflt utime stime cutime cstime priority nice num_threads itrealvalue starttime vsize rss ...
        auto open_paren = stat_str.find('(');
        auto close_paren = stat_str.rfind(')');
        if (open_paren == std::string::npos || close_paren == std::string::npos || close_paren <= open_paren) {
            continue;
        }

        ProcessInfo p;
        p.pid = pid;
        p.name = stat_str.substr(open_paren + 1, close_paren - open_paren - 1);

        std::istringstream rest(stat_str.substr(close_paren + 1));
        char state_ch = '?';
        int ppid = 0, pgrp = 0, session = 0, tty_nr = 0, tpgid = 0;
        unsigned int flags = 0;
        unsigned long minflt = 0, cminflt = 0, majflt = 0, cmajflt = 0;
        unsigned long utime = 0, stime = 0;
        long cutime = 0, cstime = 0, priority = 0, nice = 0, num_threads = 1, itrealvalue = 0;
        unsigned long long starttime = 0;
        unsigned long vsize = 0;
        long rss = 0;

        if (rest >> state_ch >> ppid >> pgrp >> session >> tty_nr >> tpgid >> flags
                 >> minflt >> cminflt >> majflt >> cmajflt >> utime >> stime
                 >> cutime >> cstime >> priority >> nice >> num_threads >> itrealvalue
                 >> starttime >> vsize >> rss) {

            p.state = static_cast<ProcessState>(state_ch);
            p.ppid = ppid;
            p.priority = static_cast<int>(priority);
            p.nice = static_cast<int>(nice);
            p.num_threads = static_cast<int>(num_threads);
            p.start_time_ticks = starttime;
            p.memory_vsz_bytes = vsize;
            // 4096 bytes per page default
            p.memory_rss_bytes = (rss > 0) ? (static_cast<uint64_t>(rss) * 4096) : 0;
        }

        // Parse /proc/[pid]/status for Uid, VmSwap
        std::string status_str = read_file_contents(pid_path + "/status");
        if (!status_str.empty()) {
            std::istringstream sstream(status_str);
            std::string sline;
            while (std::getline(sstream, sline)) {
                if (sline.rfind("Uid:", 0) == 0) {
                    std::istringstream uiss(sline.substr(4));
                    uiss >> p.uid;
                } else if (sline.rfind("VmSwap:", 0) == 0) {
                    std::istringstream wiss(sline.substr(7));
                    uint64_t swap_kb = 0;
                    if (wiss >> swap_kb) {
                        p.memory_swap_bytes = swap_kb * 1024;
                    }
                }
            }
        }

        // Read cmdline
        std::string cmd_str = read_file_contents(pid_path + "/cmdline");
        if (!cmd_str.empty()) {
            for (char& c : cmd_str) {
                if (c == '\0') c = ' ';
            }
            // Trim trailing spaces
            auto last_nb = cmd_str.find_last_not_of(' ');
            if (last_nb != std::string::npos) {
                cmd_str = cmd_str.substr(0, last_nb + 1);
            }
            p.cmdline = cmd_str;
        }

        // Kernel thread detection
        if (p.pid == 2 || p.ppid == 2 || (p.cmdline.empty() && p.ppid <= 2)) {
            p.is_kernel_thread = true;
        }

        // Read cgroup
        p.cgroup = read_file_contents(pid_path + "/cgroup");

        // Readlink exe
        std::error_code l_ec;
        std::filesystem::path exe_symlink = entry.path() / "exe";
        if (std::filesystem::exists(exe_symlink, l_ec)) {
            std::filesystem::path target = std::filesystem::read_symlink(exe_symlink, l_ec);
            if (!l_ec) {
                p.exe_path = target.string();
            }
        }

        // CPU calculation (btop++ delta approach)
        uint64_t total_proc_ticks = utime + stime;
        cur_proc_ticks[pid] = total_proc_ticks;

        auto prev_it = prev_proc_ticks_.find(pid);
        if (prev_it != prev_proc_ticks_.end() && total_proc_ticks >= prev_it->second) {
            uint64_t d_proc = total_proc_ticks - prev_it->second;
            // Convert ticks to seconds: d_proc / clk_tck_
            // Using elapsed seconds
            double proc_cpu_sec = static_cast<double>(d_proc) / clk_tck_;
            double core_count = metrics_.cores.empty() ? 1.0 : static_cast<double>(metrics_.cores.size());
            // Normalize CPU %
            p.cpu_percent = std::clamp((proc_cpu_sec / 1.0) * 100.0, 0.0, 100.0 * core_count);
        }

        proc_map_[p.pid] = p;
        processes_.push_back(p);
    }

    prev_proc_ticks_ = std::move(cur_proc_ticks);
}

} // namespace pulse::collector
