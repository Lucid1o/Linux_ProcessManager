#pragma once

#include "pulse/core/process_info.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <regex>

namespace pulse::core {

struct DesktopEntry {
    std::string id;            // e.g. "google-chrome", "code", "firefox"
    std::string name;          // e.g. "Google Chrome"
    std::string exec_name;     // Executable base name (e.g. "google-chrome-stable")
    std::string icon;          // e.g. "google-chrome"
    std::string comment;
};

class DesktopEntryDatabase {
public:
    DesktopEntryDatabase() = default;
    
    // Load entries from standard directories and custom directories (for tests)
    void load_from_directories(const std::vector<std::string>& directories);
    void add_entry(const DesktopEntry& entry);
    
    // Find entry matching an app_id or binary name
    const DesktopEntry* find_entry(const std::string& name_or_id) const;

    const std::unordered_map<std::string, DesktopEntry>& entries() const { return entries_; }

private:
    std::unordered_map<std::string, DesktopEntry> entries_;
    std::unordered_map<std::string, std::string> exec_to_entry_id_;
    
    void parse_desktop_file(const std::filesystem::path& file_path);
};

class AppGrouper {
public:
    explicit AppGrouper(DesktopEntryDatabase desktop_db = DesktopEntryDatabase());

    // Main entrypoint: groups raw processes into ApplicationGroups with hierarchical trees
    std::vector<ApplicationGroup> group_processes(const std::vector<ProcessInfo>& raw_processes);

    // Helper: heuristic A - parse cgroup app scope
    static std::string extract_cgroup_app_scope(const std::string& cgroup_content);

    // Helper: unescape systemd name (e.g., "google\x2dchrome" -> "google-chrome")
    static std::string unescape_systemd_name(const std::string& input);

    DesktopEntryDatabase& desktop_db() { return desktop_db_; }
    const DesktopEntryDatabase& desktop_db() const { return desktop_db_; }

private:
    DesktopEntryDatabase desktop_db_;

    // Builds hierarchical trees for all processes in an application group
    std::vector<std::shared_ptr<ProcessNode>> build_process_trees(
        const std::vector<int>& pids,
        const std::unordered_map<int, ProcessInfo>& proc_map);
};

} // namespace pulse::core
