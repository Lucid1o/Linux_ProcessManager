#include "pulse/collector/linux_proc_collector.hpp"
#include <cassert>
#include <iostream>

using namespace pulse::collector;
using namespace pulse::core;

#ifndef TEST_FIXTURES_DIR
#define TEST_FIXTURES_DIR "core/tests/fixtures"
#endif

int main() {
    std::string fixtures = TEST_FIXTURES_DIR;
    // Instantiate collector pointing to our fixture directory
    LinuxProcCollector collector(fixtures + "/proc", fixtures + "/sys");

    // First update (initializes baseline counters)
    collector.update();

    const auto& metrics = collector.get_system_metrics();
    assert(metrics.mem_total_bytes == 16384000ULL * 1024);
    assert(metrics.mem_available_bytes == 10485760ULL * 1024);
    assert(metrics.swap_total_bytes == 4194304ULL * 1024);

    const auto& procs = collector.get_processes();
    assert(!procs.empty());

    // Check PID 1
    auto p1 = collector.get_process_by_pid(1);
    assert(p1.has_value());
    assert(p1->name == "systemd");
    assert(p1->ppid == 0);

    // Check PID 2 (kernel thread)
    auto p2 = collector.get_process_by_pid(2);
    assert(p2.has_value());
    assert(p2->name == "kthreadd");
    assert(p2->is_kernel_thread == true);

    // Check PID 500 (Chrome browser)
    auto p500 = collector.get_process_by_pid(500);
    assert(p500.has_value());
    assert(p500->name == "chrome");
    assert(p500->memory_rss_bytes == 50000ULL * 4096);
    assert(p500->cgroup.find("app-google") != std::string::npos);

    // Check PID 600 (Zombie)
    auto p600 = collector.get_process_by_pid(600);
    assert(p600.has_value());
    assert(p600->state == ProcessState::Zombie);

    std::cout << "[PASS] test_collector with fake /proc fixture\n";
    std::cout << "All collector tests passed successfully!\n";
    return 0;
}
