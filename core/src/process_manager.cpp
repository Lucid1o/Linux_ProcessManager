#include "pulse/core/process_manager.hpp"
#include <thread>
#include <algorithm>

namespace pulse::core {

ProcessManager::ProcessManager(std::shared_ptr<IProcessController> controller)
    : controller_(std::move(controller)) {}

TerminationStatus ProcessManager::validate_signal_safety(const ProcessInfo& proc) {
    if (proc.pid <= 0) {
        return TerminationStatus::ProcessNotFound;
    }

    if (proc.pid == 1) {
        return TerminationStatus::ProtectedPid1;
    }

    if (controller_ && proc.pid == controller_->get_self_pid()) {
        return TerminationStatus::ProtectedSelf;
    }

    if (proc.is_kernel_thread || proc.pid == 2 || proc.ppid == 2) {
        return TerminationStatus::ProtectedKernelThread;
    }

    if (!controller_) {
        return TerminationStatus::Failed;
    }

    // Verify target process exists and verify start time against PID reuse
    uint64_t cur_start_time = 0;
    int cur_uid = 0;
    bool cur_kthread = false;
    if (!controller_->get_process_state(proc.pid, cur_start_time, cur_uid, cur_kthread)) {
        return TerminationStatus::ProcessNotFound;
    }

    if (cur_kthread) {
        return TerminationStatus::ProtectedKernelThread;
    }

    // Check start time for PID reuse
    if (proc.start_time_ticks > 0 && cur_start_time > 0 && proc.start_time_ticks != cur_start_time) {
        return TerminationStatus::PidReused;
    }

    // Check user permission
    int self_uid = controller_->get_self_uid();
    if (self_uid != 0 && self_uid != cur_uid) {
        return TerminationStatus::PermissionDenied;
    }

    return TerminationStatus::Success;
}

TerminationReport ProcessManager::close_normally(const ProcessInfo& target_proc) {
    TerminationReport report;
    TerminationStatus safety = validate_signal_safety(target_proc);
    if (safety != TerminationStatus::Success) {
        report.overall_status = safety;
        report.signal_results.push_back({target_proc.pid, safety, "Signal safety validation failed"});
        return report;
    }

    TerminationStatus res = controller_->send_signal(target_proc.pid, ProcessSignal::Term);
    report.overall_status = res;
    report.terminated_pids_order.push_back(target_proc.pid);
    report.signal_results.push_back({target_proc.pid, res, "Sent SIGTERM"});
    return report;
}

TerminationReport ProcessManager::force_close(const ProcessInfo& target_proc) {
    TerminationReport report;
    TerminationStatus safety = validate_signal_safety(target_proc);
    if (safety != TerminationStatus::Success) {
        report.overall_status = safety;
        report.signal_results.push_back({target_proc.pid, safety, "Signal safety validation failed"});
        return report;
    }

    TerminationStatus res = controller_->send_signal(target_proc.pid, ProcessSignal::Kill);
    report.overall_status = res;
    report.terminated_pids_order.push_back(target_proc.pid);
    report.signal_results.push_back({target_proc.pid, res, "Sent SIGKILL"});
    return report;
}

void ProcessManager::collect_leaf_first(const std::shared_ptr<ProcessNode>& node, std::vector<int>& out_pids) {
    if (!node) return;
    // Post-order traversal: visit all children first
    for (const auto& child : node->children) {
        collect_leaf_first(child, out_pids);
    }
    // Then visit this node
    out_pids.push_back(node->info.pid);
}

std::vector<int> ProcessManager::compute_leaf_first_pids(const std::vector<std::shared_ptr<ProcessNode>>& trees) {
    std::vector<int> pids;
    for (const auto& tree : trees) {
        if (!tree) continue;
        // Collect leaf-first for this tree
        std::vector<int> tree_pids;
        std::function<void(const std::shared_ptr<ProcessNode>&)> dfs = [&](const std::shared_ptr<ProcessNode>& n) {
            if (!n) return;
            for (const auto& c : n->children) {
                dfs(c);
            }
            tree_pids.push_back(n->info.pid);
        };
        dfs(tree);
        pids.insert(pids.end(), tree_pids.begin(), tree_pids.end());
    }
    return pids;
}

TerminationReport ProcessManager::close_entire_application(
    const ApplicationGroup& app,
    bool force,
    std::chrono::milliseconds grace_period,
    std::function<void(int remaining_ms)> countdown_callback) {

    TerminationReport report;
    std::vector<int> ordered_pids = compute_leaf_first_pids(app.root_process_trees);

    // If trees were empty, fallback to app.process_pids
    if (ordered_pids.empty()) {
        ordered_pids = app.process_pids;
        // Invert to approximate leaf-first (newer child processes often have higher PIDs)
        std::reverse(ordered_pids.begin(), ordered_pids.end());
    }

    report.terminated_pids_order = ordered_pids;
    ProcessSignal sig = force ? ProcessSignal::Kill : ProcessSignal::Term;

    bool any_error = false;
    for (int pid : ordered_pids) {
        // Quick construct dummy ProcessInfo for safety check
        ProcessInfo p;
        p.pid = pid;
        // Find if in app.root_process_trees
        TerminationStatus safety = validate_signal_safety(p);
        if (safety != TerminationStatus::Success) {
            report.signal_results.push_back({pid, safety, "Safety check rejected signal"});
            any_error = true;
            continue;
        }

        TerminationStatus status = controller_->send_signal(pid, sig);
        report.signal_results.push_back({pid, status, force ? "Sent SIGKILL" : "Sent SIGTERM"});
        if (status != TerminationStatus::Success) {
            any_error = true;
        }
    }

    if (!force && grace_period.count() > 0) {
        // Wait for processes to terminate gracefully
        auto start = std::chrono::steady_clock::now();
        int step_ms = 250;
        int remaining_ms = static_cast<int>(grace_period.count());

        while (remaining_ms > 0) {
            if (countdown_callback) {
                countdown_callback(remaining_ms);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(std::min(step_ms, remaining_ms)));
            
            // Check if any target process is still alive
            bool any_alive = false;
            for (int pid : ordered_pids) {
                uint64_t st = 0;
                int uid = 0;
                bool kth = false;
                if (controller_->get_process_state(pid, st, uid, kth)) {
                    any_alive = true;
                    break;
                }
            }

            if (!any_alive) {
                report.overall_status = TerminationStatus::Success;
                report.summary_message = "All application processes terminated gracefully.";
                return report;
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
            remaining_ms = static_cast<int>(grace_period.count() - elapsed.count());
        }

        report.overall_status = TerminationStatus::GraceTimeoutExpired;
        report.summary_message = "Grace period expired; some processes are still running. Escalation to SIGKILL recommended.";
        return report;
    }

    report.overall_status = any_error ? TerminationStatus::PartialSuccess : TerminationStatus::Success;
    report.summary_message = force ? "Sent SIGKILL to entire application tree." : "Sent SIGTERM to application processes.";
    return report;
}

} // namespace pulse::core
