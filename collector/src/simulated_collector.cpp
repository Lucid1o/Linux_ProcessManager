#include "pulse/collector/simulated_collector.hpp"
#include <cmath>
#include <random>
#include <algorithm>
#include <iostream>

namespace pulse::collector {

SimulatedCollector::SimulatedCollector()
    : start_time_(std::chrono::steady_clock::now()) {
    init_sample_processes();
}

void SimulatedCollector::init_sample_processes() {
    proc_map_.clear();
    processes_.clear();

    // 1. Google Chrome (Application with 4 processes)
    ProcessInfo chrome_main;
    chrome_main.pid = 1010;
    chrome_main.ppid = 1;
    chrome_main.name = "chrome";
    chrome_main.cmdline = "/usr/bin/google-chrome-stable --ozone-platform-hint=auto";
    chrome_main.exe_path = "/opt/google/chrome/google-chrome";
    chrome_main.user = "user";
    chrome_main.uid = 1000;
    chrome_main.state = pulse::core::ProcessState::Sleeping;
    chrome_main.priority = 20;
    chrome_main.nice = 0;
    chrome_main.num_threads = 32;
    chrome_main.cpu_percent = 4.2;
    chrome_main.memory_rss_bytes = 1024ULL * 1024 * 180; // 180MB
    chrome_main.memory_vsz_bytes = 1024ULL * 1024 * 1200;
    chrome_main.start_time_ticks = 1000;
    chrome_main.cgroup = "0::/user.slice/user-1000.slice/user@1000.service/app.slice/app-google\\x2dchrome-1010.scope";
    proc_map_[chrome_main.pid] = chrome_main;

    ProcessInfo chrome_gpu;
    chrome_gpu.pid = 1011;
    chrome_gpu.ppid = 1010;
    chrome_gpu.name = "chrome";
    chrome_gpu.cmdline = "/opt/google/chrome/chrome --type=gpu-process";
    chrome_gpu.exe_path = "/opt/google/chrome/google-chrome";
    chrome_gpu.user = "user";
    chrome_gpu.uid = 1000;
    chrome_gpu.state = pulse::core::ProcessState::Sleeping;
    chrome_gpu.priority = 20;
    chrome_gpu.nice = 0;
    chrome_gpu.num_threads = 16;
    chrome_gpu.cpu_percent = 2.1;
    chrome_gpu.memory_rss_bytes = 1024ULL * 1024 * 240;
    chrome_gpu.memory_vsz_bytes = 1024ULL * 1024 * 800;
    chrome_gpu.start_time_ticks = 1002;
    chrome_gpu.cgroup = chrome_main.cgroup;
    proc_map_[chrome_gpu.pid] = chrome_gpu;

    ProcessInfo chrome_render1;
    chrome_render1.pid = 1012;
    chrome_render1.ppid = 1010;
    chrome_render1.name = "chrome";
    chrome_render1.cmdline = "/opt/google/chrome/chrome --type=renderer --extension-process";
    chrome_render1.exe_path = "/opt/google/chrome/google-chrome";
    chrome_render1.user = "user";
    chrome_render1.uid = 1000;
    chrome_render1.state = pulse::core::ProcessState::Running;
    chrome_render1.priority = 20;
    chrome_render1.nice = 0;
    chrome_render1.num_threads = 12;
    chrome_render1.cpu_percent = 7.5;
    chrome_render1.memory_rss_bytes = 1024ULL * 1024 * 320;
    chrome_render1.memory_vsz_bytes = 1024ULL * 1024 * 950;
    chrome_render1.start_time_ticks = 1005;
    chrome_render1.cgroup = chrome_main.cgroup;
    proc_map_[chrome_render1.pid] = chrome_render1;

    ProcessInfo chrome_render2;
    chrome_render2.pid = 1013;
    chrome_render2.ppid = 1010;
    chrome_render2.name = "chrome";
    chrome_render2.cmdline = "/opt/google/chrome/chrome --type=renderer --site-per-process";
    chrome_render2.exe_path = "/opt/google/chrome/google-chrome";
    chrome_render2.user = "user";
    chrome_render2.uid = 1000;
    chrome_render2.state = pulse::core::ProcessState::Sleeping;
    chrome_render2.priority = 20;
    chrome_render2.nice = 0;
    chrome_render2.num_threads = 8;
    chrome_render2.cpu_percent = 1.0;
    chrome_render2.memory_rss_bytes = 1024ULL * 1024 * 160;
    chrome_render2.memory_vsz_bytes = 1024ULL * 1024 * 700;
    chrome_render2.start_time_ticks = 1006;
    chrome_render2.cgroup = chrome_main.cgroup;
    proc_map_[chrome_render2.pid] = chrome_render2;

    // 2. Visual Studio Code
    ProcessInfo code_main;
    code_main.pid = 2020;
    code_main.ppid = 1;
    code_main.name = "code";
    code_main.cmdline = "/usr/share/code/code .";
    code_main.exe_path = "/usr/share/code/code";
    code_main.user = "user";
    code_main.uid = 1000;
    code_main.state = pulse::core::ProcessState::Sleeping;
    code_main.priority = 20;
    code_main.nice = 0;
    code_main.num_threads = 24;
    code_main.cpu_percent = 3.5;
    code_main.memory_rss_bytes = 1024ULL * 1024 * 220;
    code_main.memory_vsz_bytes = 1024ULL * 1024 * 1500;
    code_main.start_time_ticks = 2000;
    code_main.cgroup = "0::/user.slice/user-1000.slice/user@1000.service/app.slice/app-code-2020.scope";
    proc_map_[code_main.pid] = code_main;

    ProcessInfo code_ext;
    code_ext.pid = 2021;
    code_ext.ppid = 2020;
    code_ext.name = "code";
    code_ext.cmdline = "/usr/share/code/code --type=extensionHost";
    code_ext.exe_path = "/usr/share/code/code";
    code_ext.user = "user";
    code_ext.uid = 1000;
    code_ext.state = pulse::core::ProcessState::Sleeping;
    code_ext.priority = 20;
    code_ext.nice = 0;
    code_ext.num_threads = 10;
    code_ext.cpu_percent = 1.8;
    code_ext.memory_rss_bytes = 1024ULL * 1024 * 140;
    code_ext.memory_vsz_bytes = 1024ULL * 1024 * 800;
    code_ext.start_time_ticks = 2002;
    code_ext.cgroup = code_main.cgroup;
    proc_map_[code_ext.pid] = code_ext;

    // 3. Spotify
    ProcessInfo spotify_main;
    spotify_main.pid = 3030;
    spotify_main.ppid = 1;
    spotify_main.name = "spotify";
    spotify_main.cmdline = "/usr/bin/spotify";
    spotify_main.exe_path = "/usr/share/spotify/spotify";
    spotify_main.user = "user";
    spotify_main.uid = 1000;
    spotify_main.state = pulse::core::ProcessState::Sleeping;
    spotify_main.priority = 20;
    spotify_main.nice = 0;
    spotify_main.num_threads = 18;
    spotify_main.cpu_percent = 2.4;
    spotify_main.memory_rss_bytes = 1024ULL * 1024 * 190;
    spotify_main.memory_vsz_bytes = 1024ULL * 1024 * 900;
    spotify_main.start_time_ticks = 3000;
    spotify_main.cgroup = "0::/user.slice/user-1000.slice/user@1000.service/app.slice/app-spotify-3030.scope";
    proc_map_[spotify_main.pid] = spotify_main;

    // 4. Background daemons / system processes
    ProcessInfo init_proc;
    init_proc.pid = 1;
    init_proc.ppid = 0;
    init_proc.name = "systemd";
    init_proc.cmdline = "/sbin/init";
    init_proc.exe_path = "/lib/systemd/systemd";
    init_proc.user = "root";
    init_proc.uid = 0;
    init_proc.state = pulse::core::ProcessState::Sleeping;
    init_proc.priority = 20;
    init_proc.nice = 0;
    init_proc.num_threads = 1;
    init_proc.cpu_percent = 0.1;
    init_proc.memory_rss_bytes = 1024ULL * 1024 * 16;
    init_proc.memory_vsz_bytes = 1024ULL * 1024 * 160;
    init_proc.start_time_ticks = 1;
    init_proc.cgroup = "0::/";
    proc_map_[init_proc.pid] = init_proc;

    ProcessInfo kthreadd;
    kthreadd.pid = 2;
    kthreadd.ppid = 0;
    kthreadd.name = "kthreadd";
    kthreadd.cmdline = "";
    kthreadd.is_kernel_thread = true;
    kthreadd.user = "root";
    kthreadd.uid = 0;
    kthreadd.state = pulse::core::ProcessState::Sleeping;
    kthreadd.priority = 20;
    kthreadd.nice = 0;
    kthreadd.num_threads = 1;
    kthreadd.cpu_percent = 0.0;
    kthreadd.start_time_ticks = 2;
    proc_map_[kthreadd.pid] = kthreadd;

    // 5. Test Zombie process (to trigger rule 3: Zombie detected)
    ProcessInfo zombie_proc;
    zombie_proc.pid = 4040;
    zombie_proc.ppid = 1010;
    zombie_proc.name = "chrome_crash_reporter <defunct>";
    zombie_proc.cmdline = "";
    zombie_proc.user = "user";
    zombie_proc.uid = 1000;
    zombie_proc.state = pulse::core::ProcessState::Zombie;
    zombie_proc.priority = 20;
    zombie_proc.nice = 0;
    zombie_proc.num_threads = 0;
    zombie_proc.cpu_percent = 0.0;
    zombie_proc.memory_rss_bytes = 0;
    zombie_proc.start_time_ticks = 4000;
    zombie_proc.cgroup = chrome_main.cgroup;
    proc_map_[zombie_proc.pid] = zombie_proc;

    // 6. Test high-memory process (to trigger rule 1: > 25% RAM)
    ProcessInfo hog_proc;
    hog_proc.pid = 5050;
    hog_proc.ppid = 1;
    hog_proc.name = "render_simulation";
    hog_proc.cmdline = "/opt/render_sim/bin/render_simulation --batch-size=64";
    hog_proc.exe_path = "/opt/render_sim/bin/render_simulation";
    hog_proc.user = "user";
    hog_proc.uid = 1000;
    hog_proc.state = pulse::core::ProcessState::Running;
    hog_proc.priority = 20;
    hog_proc.nice = 0;
    hog_proc.num_threads = 16;
    hog_proc.cpu_percent = 45.0;
    hog_proc.memory_rss_bytes = 1024ULL * 1024 * 1024 * 5; // 5 GB (~31% of 16 GB)
    hog_proc.memory_vsz_bytes = 1024ULL * 1024 * 1024 * 8;
    hog_proc.start_time_ticks = 5000;
    hog_proc.cgroup = "0::/user.slice/user-1000.slice/user@1000.service/app.slice/app-render_sim-5050.scope";
    proc_map_[hog_proc.pid] = hog_proc;

    // Convert map to vector
    processes_.clear();
    for (const auto& [pid, p] : proc_map_) {
        processes_.push_back(p);
    }
}

void SimulatedCollector::update() {
    tick_counter_++;

    // Realistic system metrics fluctuation
    metrics_.mem_total_bytes = 16ULL * 1024 * 1024 * 1024; // 16 GB RAM
    metrics_.swap_total_bytes = 4ULL * 1024 * 1024 * 1024; // 4 GB Swap

    double t = static_cast<double>(tick_counter_);
    // Sine waves + noise for dynamic charts
    double cpu_base = 18.0 + 8.0 * std::sin(t * 0.15);
    metrics_.total_cpu_percent = std::clamp(cpu_base, 2.0, 95.0);

    if (metrics_.cores.empty()) {
        metrics_.cores.resize(8);
        for (int i = 0; i < 8; ++i) {
            metrics_.cores[i].core_id = i;
        }
    }
    for (size_t i = 0; i < metrics_.cores.size(); ++i) {
        double c_val = cpu_base + 5.0 * std::sin(t * 0.2 + i * 0.7);
        metrics_.cores[i].usage_percent = std::clamp(c_val, 1.0, 99.0);
    }

    uint64_t base_used = 7ULL * 1024 * 1024 * 1024;
    int64_t ram_fluct = static_cast<int64_t>((std::sin(t * 0.1) * 300) * 1024 * 1024);
    metrics_.mem_used_bytes = static_cast<uint64_t>(base_used + ram_fluct);
    metrics_.mem_available_bytes = metrics_.mem_total_bytes - metrics_.mem_used_bytes;

    metrics_.swap_used_bytes = 512ULL * 1024 * 1024 + static_cast<uint64_t>(std::sin(t * 0.05) * 50 * 1024 * 1024);
    metrics_.swap_free_bytes = metrics_.swap_total_bytes - metrics_.swap_used_bytes;

    // Disk I/O: 10 MB/s to 45 MB/s
    metrics_.disk.read_bytes_per_sec = static_cast<uint64_t>((15.0 + 10.0 * std::cos(t * 0.25)) * 1024 * 1024);
    metrics_.disk.write_bytes_per_sec = static_cast<uint64_t>((8.0 + 6.0 * std::sin(t * 0.3)) * 1024 * 1024);
    metrics_.disk.total_read_bytes += metrics_.disk.read_bytes_per_sec;
    metrics_.disk.total_write_bytes += metrics_.disk.write_bytes_per_sec;

    // Network: 1 MB/s to 12 MB/s
    metrics_.network.rx_bytes_per_sec = static_cast<uint64_t>((4.0 + 3.0 * std::sin(t * 0.4)) * 1024 * 1024);
    metrics_.network.tx_bytes_per_sec = static_cast<uint64_t>((1.5 + 1.0 * std::cos(t * 0.35)) * 1024 * 1024);
    metrics_.network.total_rx_bytes += metrics_.network.rx_bytes_per_sec;
    metrics_.network.total_tx_bytes += metrics_.network.tx_bytes_per_sec;

    // Minor fluctuations in process CPU
    for (auto& [pid, p] : proc_map_) {
        if (p.cpu_percent > 0.0) {
            double var = (std::sin(t + pid) * 0.8);
            p.cpu_percent = std::max(0.1, p.cpu_percent + var);
        }
    }

    processes_.clear();
    for (const auto& [pid, p] : proc_map_) {
        processes_.push_back(p);
    }

    metrics_.total_processes = static_cast<int>(processes_.size());
    int total_threads = 0;
    for (const auto& p : processes_) {
        total_threads += p.num_threads;
    }
    metrics_.total_threads = total_threads;
}

std::optional<ProcessInfo> SimulatedCollector::get_process_by_pid(int pid) const {
    auto it = proc_map_.find(pid);
    if (it != proc_map_.end()) return it->second;
    return std::nullopt;
}

bool SimulatedCollector::get_process_state(int pid, uint64_t& out_start_time, int& out_uid, bool& out_is_kthread) {
    auto it = proc_map_.find(pid);
    if (it == proc_map_.end()) {
        return false;
    }
    out_start_time = it->second.start_time_ticks;
    out_uid = it->second.uid;
    out_is_kthread = it->second.is_kernel_thread;
    return true;
}

pulse::core::TerminationStatus SimulatedCollector::send_signal(int pid, pulse::core::ProcessSignal signal) {
    auto it = proc_map_.find(pid);
    if (it == proc_map_.end()) {
        return pulse::core::TerminationStatus::ProcessNotFound;
    }

    if (signal == pulse::core::ProcessSignal::Term || signal == pulse::core::ProcessSignal::Kill) {
        // Remove from process map (simulating termination)
        proc_map_.erase(it);
        // Rebuild list
        processes_.clear();
        for (const auto& [id, p] : proc_map_) {
            processes_.push_back(p);
        }
    }

    return pulse::core::TerminationStatus::Success;
}

void SimulatedCollector::inject_zombie_process(int pid, const std::string& name, int ppid) {
    ProcessInfo z;
    z.pid = pid;
    z.ppid = ppid;
    z.name = name;
    z.state = pulse::core::ProcessState::Zombie;
    z.start_time_ticks = 9999;
    z.uid = 1000;
    proc_map_[pid] = z;
    processes_.push_back(z);
}

void SimulatedCollector::inject_high_memory_process(int pid, const std::string& name, uint64_t ram_bytes) {
    ProcessInfo h;
    h.pid = pid;
    h.ppid = 1;
    h.name = name;
    h.memory_rss_bytes = ram_bytes;
    h.start_time_ticks = 8888;
    h.uid = 1000;
    proc_map_[pid] = h;
    processes_.push_back(h);
}

} // namespace pulse::collector
