#include "pulse/core/process_manager.hpp"
#include "pulse/core/app_grouper.hpp"
#include <cassert>
#include <iostream>
#include <unordered_map>

using namespace pulse::core;

class MockProcessController : public IProcessController {
public:
    struct MockProc {
        uint64_t start_time{1000};
        int uid{1000};
        bool is_kthread{false};
        bool alive{true};
    };

    std::unordered_map<int, MockProc> procs;
    std::vector<std::pair<int, ProcessSignal>> signal_log;
    int self_pid{9999};
    int self_uid{1000};

    bool get_process_state(int pid, uint64_t& out_start_time, int& out_uid, bool& out_is_kthread) override {
        auto it = procs.find(pid);
        if (it == procs.end() || !it->second.alive) return false;
        out_start_time = it->second.start_time;
        out_uid = it->second.uid;
        out_is_kthread = it->second.is_kthread;
        return true;
    }

    TerminationStatus send_signal(int pid, ProcessSignal signal) override {
        signal_log.emplace_back(pid, signal);
        auto it = procs.find(pid);
        if (it == procs.end() || !it->second.alive) {
            return TerminationStatus::ProcessNotFound;
        }
        if (signal == ProcessSignal::Kill || signal == ProcessSignal::Term) {
            it->second.alive = false;
        }
        return TerminationStatus::Success;
    }

    int get_self_pid() const override { return self_pid; }
    int get_self_uid() const override { return self_uid; }
};

void test_leaf_first_order() {
    auto mock = std::make_shared<MockProcessController>();
    mock->procs[500] = {5000, 1000, false, true};
    mock->procs[501] = {5010, 1000, false, true};
    mock->procs[502] = {5020, 1000, false, true};

    // Tree: 500 -> 501 -> 502
    auto n500 = std::make_shared<ProcessNode>();
    n500->info.pid = 500;
    n500->info.start_time_ticks = 5000;

    auto n501 = std::make_shared<ProcessNode>();
    n501->info.pid = 501;
    n501->info.start_time_ticks = 5010;

    auto n502 = std::make_shared<ProcessNode>();
    n502->info.pid = 502;
    n502->info.start_time_ticks = 5020;

    n501->children.push_back(n502);
    n500->children.push_back(n501);

    ApplicationGroup app;
    app.app_id = "test-app";
    app.root_process_trees.push_back(n500);

    ProcessManager mgr(mock);
    auto report = mgr.close_entire_application(app, true, std::chrono::milliseconds(0));

    assert(report.overall_status == TerminationStatus::Success);
    assert(report.terminated_pids_order.size() == 3);

    // Leaf first verification: 502 first, then 501, then 500!
    assert(report.terminated_pids_order[0] == 502);
    assert(report.terminated_pids_order[1] == 501);
    assert(report.terminated_pids_order[2] == 500);

    std::cout << "[PASS] test_leaf_first_order\n";
}

void test_safety_protections() {
    auto mock = std::make_shared<MockProcessController>();
    mock->procs[1] = {1, 0, false, true};             // systemd
    mock->procs[2] = {2, 0, true, true};              // kthread
    mock->procs[9999] = {999, 1000, false, true};      // Pulse self
    mock->procs[1234] = {1234, 0, false, true};        // Root-owned process
    mock->procs[2345] = {5000, 1000, false, true};     // Live user process

    ProcessManager mgr(mock);

    // 1. PID 1 protection
    ProcessInfo p1;
    p1.pid = 1;
    auto rep1 = mgr.close_normally(p1);
    assert(rep1.overall_status == TerminationStatus::ProtectedPid1);

    // 2. Kernel thread protection
    ProcessInfo p2;
    p2.pid = 2;
    p2.is_kernel_thread = true;
    auto rep2 = mgr.close_normally(p2);
    assert(rep2.overall_status == TerminationStatus::ProtectedKernelThread);

    // 3. Pulse self protection
    ProcessInfo p_self;
    p_self.pid = 9999;
    auto rep_self = mgr.close_normally(p_self);
    assert(rep_self.overall_status == TerminationStatus::ProtectedSelf);

    // 4. Permission denied (other user process, caller is non-root uid 1000)
    ProcessInfo p_root;
    p_root.pid = 1234;
    p_root.uid = 0;
    p_root.start_time_ticks = 1234;
    auto rep_perm = mgr.close_normally(p_root);
    assert(rep_perm.overall_status == TerminationStatus::PermissionDenied);

    // 5. PID reuse prevention (start time mismatch)
    ProcessInfo p_reused;
    p_reused.pid = 2345;
    p_reused.uid = 1000;
    p_reused.start_time_ticks = 1111; // Mismatch with mock's 5000!
    auto rep_reuse = mgr.close_normally(p_reused);
    assert(rep_reuse.overall_status == TerminationStatus::PidReused);

    // 6. Process not found (ESRCH)
    ProcessInfo p_nonexist;
    p_nonexist.pid = 88888;
    auto rep_esrch = mgr.close_normally(p_nonexist);
    assert(rep_esrch.overall_status == TerminationStatus::ProcessNotFound);

    std::cout << "[PASS] test_safety_protections\n";
}

int main() {
    test_leaf_first_order();
    test_safety_protections();
    std::cout << "All termination and safety tests passed successfully!\n";
    return 0;
}
