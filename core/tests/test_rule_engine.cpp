#include "pulse/core/rule_engine.hpp"
#include <cassert>
#include <iostream>

using namespace pulse::core;

void test_duration_and_threshold() {
    RuleEngine engine;
    engine.load_default_rules();

    // 16 GB Total RAM
    SystemMetrics metrics;
    metrics.mem_total_bytes = 16ULL * 1024 * 1024 * 1024;

    // Chrome app consuming 5 GB (31.25% > 25% threshold)
    ApplicationGroup chrome;
    chrome.app_id = "google-chrome";
    chrome.display_name = "Google Chrome";
    chrome.total_memory_rss_bytes = 5ULL * 1024 * 1024 * 1024;
    chrome.root_pid = 1010;

    std::vector<ApplicationGroup> apps = { chrome };
    std::vector<ProcessInfo> procs;

    auto t0 = std::chrono::steady_clock::now();

    // Tick 0: condition met, but elapsed = 0 < 300 seconds -> No recommendation
    auto recs0 = engine.evaluate(apps, procs, metrics, t0);
    assert(recs0.empty());

    // Tick 1: 150 seconds elapsed -> Still no recommendation (< 300s)
    auto t1 = t0 + std::chrono::seconds(150);
    auto recs1 = engine.evaluate(apps, procs, metrics, t1);
    assert(recs1.empty());

    // Tick 2: 301 seconds elapsed -> Recommendation triggers!
    auto t2 = t0 + std::chrono::seconds(301);
    auto recs2 = engine.evaluate(apps, procs, metrics, t2);
    assert(recs2.size() == 1);
    assert(recs2[0].rule_id == "high_memory_duration");
    assert(recs2[0].entity_id == "google-chrome");
    std::cout << "Triggered alert message: " << recs2[0].message << "\n";

    // Dismiss recommendation
    engine.dismiss_recommendation(recs2[0].id);
    auto recs_dismissed = engine.evaluate(apps, procs, metrics, t2 + std::chrono::seconds(10));
    assert(recs_dismissed.empty());

    std::cout << "[PASS] test_duration_and_threshold\n";
}

void test_zombie_rule() {
    RuleEngine engine;
    engine.load_default_rules();

    SystemMetrics metrics;
    std::vector<ApplicationGroup> apps;
    std::vector<ProcessInfo> procs;

    ProcessInfo z;
    z.pid = 4040;
    z.name = "defunct_task";
    z.state = ProcessState::Zombie;
    procs.push_back(z);

    auto recs = engine.evaluate(apps, procs, metrics, std::chrono::steady_clock::now());
    assert(recs.size() == 1);
    assert(recs[0].rule_id == "zombie_process_detected");
    assert(recs[0].entity_id == "4040");
    std::cout << "Triggered zombie message: " << recs[0].message << "\n";

    std::cout << "[PASS] test_zombie_rule\n";
}

int main() {
    test_duration_and_threshold();
    test_zombie_rule();
    std::cout << "All rule engine tests passed successfully!\n";
    return 0;
}
