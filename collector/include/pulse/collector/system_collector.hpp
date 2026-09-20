#pragma once

#include "pulse/core/process_info.hpp"
#include <vector>
#include <optional>
#include <memory>

namespace pulse::collector {

using pulse::core::SystemMetrics;
using pulse::core::ProcessInfo;

class ISystemCollector {
public:
    virtual ~ISystemCollector() = default;

    // Refresh all system metrics and process entries
    virtual void update() = 0;

    // Retrieve system-wide metrics (CPU, Memory, Disk, Net)
    virtual const SystemMetrics& get_system_metrics() const = 0;

    // Retrieve list of all currently active processes
    virtual const std::vector<ProcessInfo>& get_processes() const = 0;

    // Fast lookup for single process info
    virtual std::optional<ProcessInfo> get_process_by_pid(int pid) const = 0;
};

} // namespace pulse::collector
