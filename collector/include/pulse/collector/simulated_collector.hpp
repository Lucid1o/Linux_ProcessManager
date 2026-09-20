#pragma once

#include "pulse/collector/system_collector.hpp"
#include "pulse/core/process_manager.hpp"
#include <vector>
#include <unordered_map>
#include <chrono>

namespace pulse::collector {

class SimulatedCollector : public ISystemCollector, public pulse::core::IProcessController {
public:
    SimulatedCollector();

    // ISystemCollector
    void update() override;
    const SystemMetrics& get_system_metrics() const override { return metrics_; }
    const std::vector<ProcessInfo>& get_processes() const override { return processes_; }
    std::optional<ProcessInfo> get_process_by_pid(int pid) const override;

    // IProcessController
    bool get_process_state(int pid, uint64_t& out_start_time, int& out_uid, bool& out_is_kthread) override;
    pulse::core::TerminationStatus send_signal(int pid, pulse::core::ProcessSignal signal) override;
    int get_self_pid() const override { return 99999; }
    int get_self_uid() const override { return 1000; }

    // Test hooks
    void inject_zombie_process(int pid, const std::string& name, int ppid);
    void inject_high_memory_process(int pid, const std::string& name, uint64_t ram_bytes);

private:
    SystemMetrics metrics_;
    std::vector<ProcessInfo> processes_;
    std::unordered_map<int, ProcessInfo> proc_map_;
    std::chrono::steady_clock::time_point start_time_;
    uint64_t tick_counter_{0};

    void init_sample_processes();
};

} // namespace pulse::collector
