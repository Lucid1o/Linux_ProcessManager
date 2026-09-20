#pragma once

#include "pulse/core/process_info.hpp"
#include <string>
#include <vector>
#include <functional>
#include <system_error>
#include <chrono>

namespace pulse::core {

enum class TerminationStatus {
    Success,
    ProcessNotFound,         // ESRCH or PID does not exist
    PermissionDenied,        // EPERM
    ProtectedPid1,           // Denied: systemd / init (PID 1)
    ProtectedSelf,           // Denied: Pulse's own PID
    ProtectedKernelThread,   // Denied: kernel thread (kthread / PID 2 / PPID 2)
    PidReused,               // Start time changed; avoided sending signal to recycled PID
    GraceTimeoutExpired,     // Application did not exit within grace timeout
    PartialSuccess,          // Some children terminated, others failed
    Failed                   // Generic failure
};

struct SignalResult {
    int pid{0};
    TerminationStatus status{TerminationStatus::Success};
    std::string message;
};

struct TerminationReport {
    TerminationStatus overall_status{TerminationStatus::Success};
    std::vector<SignalResult> signal_results;
    std::vector<int> terminated_pids_order; // Sequence of PIDs signaled (children first)
    std::string summary_message;
};

// Interface for dispatching low-level OS process signals
class IProcessController {
public:
    virtual ~IProcessController() = default;

    // Check if a process exists and fetch its start_time_ticks and uid
    virtual bool get_process_state(int pid, uint64_t& out_start_time, int& out_uid, bool& out_is_kthread) = 0;

    // Send a POSIX signal to a process
    virtual TerminationStatus send_signal(int pid, ProcessSignal signal) = 0;

    // Get the caller's own PID
    virtual int get_self_pid() const = 0;

    // Get the caller's current UID
    virtual int get_self_uid() const = 0;
};

class ProcessManager {
public:
    explicit ProcessManager(std::shared_ptr<IProcessController> controller);

    // Close Normally: sends SIGTERM to the app's root process
    TerminationReport close_normally(const ProcessInfo& target_proc);

    // Force Close: sends SIGKILL directly
    TerminationReport force_close(const ProcessInfo& target_proc);

    // Close Entire Application: leaf-first termination of the entire tree
    TerminationReport close_entire_application(
        const ApplicationGroup& app,
        bool force = false,
        std::chrono::milliseconds grace_period = std::chrono::milliseconds(5000),
        std::function<void(int remaining_ms)> countdown_callback = nullptr);

    // Computes the leaf-first post-order traversal of PIDs in a process tree
    static std::vector<int> compute_leaf_first_pids(const std::vector<std::shared_ptr<ProcessNode>>& trees);

    // Safety checks before sending any signal
    TerminationStatus validate_signal_safety(const ProcessInfo& proc);

private:
    std::shared_ptr<IProcessController> controller_;

    void collect_leaf_first(const std::shared_ptr<ProcessNode>& node, std::vector<int>& out_pids);
};

} // namespace pulse::core
