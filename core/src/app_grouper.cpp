#include "pulse/core/app_grouper.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace pulse::core {

void DesktopEntryDatabase::add_entry(const DesktopEntry& entry) {
    entries_[entry.id] = entry;
    if (!entry.exec_name.empty()) {
        exec_to_entry_id_[entry.exec_name] = entry.id;
    }
}

void DesktopEntryDatabase::load_from_directories(const std::vector<std::string>& directories) {
    for (const auto& dir_str : directories) {
        std::error_code ec;
        std::filesystem::path dir_path(dir_str);
        if (!std::filesystem::exists(dir_path, ec) || !std::filesystem::is_directory(dir_path, ec)) {
            continue;
        }

        for (const auto& entry : std::filesystem::directory_iterator(dir_path, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".desktop") {
                parse_desktop_file(entry.path());
            }
        }
    }
}

void DesktopEntryDatabase::parse_desktop_file(const std::filesystem::path& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) return;

    DesktopEntry entry;
    entry.id = file_path.stem().string();

    std::string line;
    bool in_desktop_entry = false;

    while (std::getline(file, line)) {
        // Trim leading/trailing whitespace
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        auto end = line.find_last_not_of(" \t\r\n");
        std::string trimmed = line.substr(start, end - start + 1);

        if (trimmed == "[Desktop Entry]") {
            in_desktop_entry = true;
            continue;
        } else if (trimmed.front() == '[' && trimmed.back() == ']') {
            in_desktop_entry = false;
            continue;
        }

        if (!in_desktop_entry) continue;

        auto eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = trimmed.substr(0, eq_pos);
        std::string value = trimmed.substr(eq_pos + 1);

        if (key == "Name" && entry.name.empty()) {
            entry.name = value;
        } else if (key == "Icon" && entry.icon.empty()) {
            entry.icon = value;
        } else if (key == "Comment" && entry.comment.empty()) {
            entry.comment = value;
        } else if (key == "Exec" && entry.exec_name.empty()) {
            // Exec could be "/usr/bin/google-chrome-stable %U"
            std::istringstream iss(value);
            std::string first_token;
            if (iss >> first_token) {
                // Extract filename component
                std::filesystem::path exec_p(first_token);
                entry.exec_name = exec_p.filename().string();
            }
        }
    }

    if (entry.name.empty()) {
        entry.name = entry.id;
    }

    add_entry(entry);
}

const DesktopEntry* DesktopEntryDatabase::find_entry(const std::string& name_or_id) const {
    auto it = entries_.find(name_or_id);
    if (it != entries_.end()) {
        return &it->second;
    }

    auto it_exec = exec_to_entry_id_.find(name_or_id);
    if (it_exec != exec_to_entry_id_.end()) {
        auto it_found = entries_.find(it_exec->second);
        if (it_found != entries_.end()) {
            return &it_found->second;
        }
    }

    return nullptr;
}

AppGrouper::AppGrouper(DesktopEntryDatabase desktop_db)
    : desktop_db_(std::move(desktop_db)) {
    // If desktop DB is empty, try default Linux search paths
    if (desktop_db_.entries().empty()) {
        std::vector<std::string> default_dirs = {
            "/usr/share/applications",
            "/usr/local/share/applications",
            "/var/lib/flatpak/exports/share/applications"
        };
        const char* home = std::getenv("HOME");
        if (home) {
            default_dirs.push_back(std::string(home) + "/.local/share/applications");
        }
        desktop_db_.load_from_directories(default_dirs);
    }
}

std::string AppGrouper::unescape_systemd_name(const std::string& input) {
    std::string result;
    result.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\\' && i + 3 < input.size() && input[i+1] == 'x') {
            std::string hex = input.substr(i + 2, 2);
            char* end_ptr = nullptr;
            long val = std::strtol(hex.c_str(), &end_ptr, 16);
            if (end_ptr == hex.c_str() + 2) {
                result.push_back(static_cast<char>(val));
                i += 3;
                continue;
            }
        }
        result.push_back(input[i]);
    }
    return result;
}

std::string AppGrouper::extract_cgroup_app_scope(const std::string& cgroup_content) {
    // Look for app-*.scope or app-flatpak-*.scope or app-gnome-*.scope
    // Examples:
    // 0::/user.slice/user-1000.slice/user@1000.service/app.slice/app-google\x2dchrome-1234.scope
    // 0::/user.slice/user-1000.slice/user@1000.service/app.slice/app-gnome-org.gnome.Terminal-1234.scope
    // 0::/system.slice/docker.service
    std::istringstream stream(cgroup_content);
    std::string line;
    while (std::getline(stream, line)) {
        auto app_pos = line.find("/app-");
        if (app_pos == std::string::npos) {
            app_pos = line.find("app-");
        }
        if (app_pos != std::string::npos) {
            size_t start = app_pos;
            if (line[start] == '/') start++;
            start += 4; // skip "app-"

            size_t end = line.find(".scope", start);
            if (end != std::string::npos) {
                std::string raw_scope = line.substr(start, end - start);
                // raw_scope might be: "google\x2dchrome-1234" or "gnome-org.gnome.Terminal-1234"
                // If it starts with "flatpak-" or "gnome-", strip prefix
                if (raw_scope.rfind("flatpak-", 0) == 0) {
                    raw_scope = raw_scope.substr(8);
                } else if (raw_scope.rfind("gnome-", 0) == 0) {
                    raw_scope = raw_scope.substr(6);
                }

                // Strip trailing invocation random ID: usually "-<random_id>"
                auto last_dash = raw_scope.rfind('-');
                if (last_dash != std::string::npos && last_dash > 0) {
                    raw_scope = raw_scope.substr(0, last_dash);
                }

                return unescape_systemd_name(raw_scope);
            }
        }
    }
    return "";
}

std::vector<ApplicationGroup> AppGrouper::group_processes(const std::vector<ProcessInfo>& raw_processes) {
    if (raw_processes.empty()) return {};

    std::unordered_map<int, ProcessInfo> proc_map;
    proc_map.reserve(raw_processes.size());
    for (const auto& proc : raw_processes) {
        proc_map[proc.pid] = proc;
    }

    // Determine the root process for each PID by climbing the process tree (Heuristic B)
    // An ancestor is considered an "app root" if:
    // - PPID == 0 or PPID == 1 or PPID not in map
    // - or parent is systemd/init
    // - or parent belongs to a different desktop session
    auto find_app_root_pid = [&](int pid) -> int {
        int curr = pid;
        int max_depth = 64; // Guard against potential cycles
        while (max_depth-- > 0) {
            auto it = proc_map.find(curr);
            if (it == proc_map.end()) break;
            int parent_pid = it->second.ppid;
            if (parent_pid <= 1) {
                return curr; // curr is root
            }
            auto p_it = proc_map.find(parent_pid);
            if (p_it == proc_map.end()) {
                return curr; // parent not in our process list
            }
            // If parent is PID 1 or systemd/init/launchd, curr is the application root
            if (p_it->second.name == "systemd" || p_it->second.name == "init") {
                return curr;
            }
            curr = parent_pid;
        }
        return curr;
    };

    // Intermediate bucket for grouping
    struct GroupBucket {
        std::string app_id;
        std::string display_name;
        std::string icon_name;
        int root_pid{0};
        std::vector<int> pids;
    };

    std::unordered_map<std::string, GroupBucket> groups;

    for (const auto& proc : raw_processes) {
        // Step a: Check systemd cgroup app scope
        std::string app_id = extract_cgroup_app_scope(proc.cgroup);
        int root_pid = 0;

        // Step b: If no cgroup app scope, walk the process tree to app root
        if (app_id.empty()) {
            root_pid = find_app_root_pid(proc.pid);
            auto root_it = proc_map.find(root_pid);
            if (root_it != proc_map.end()) {
                // Try cgroup of root process
                app_id = extract_cgroup_app_scope(root_it->second.cgroup);
                if (app_id.empty()) {
                    // Use root process executable name or comm
                    if (!root_it->second.exe_path.empty()) {
                        std::filesystem::path p(root_it->second.exe_path);
                        app_id = p.filename().string();
                    }
                    if (app_id.empty()) {
                        app_id = root_it->second.name;
                    }
                }
            } else {
                app_id = proc.name;
                root_pid = proc.pid;
            }
        } else {
            root_pid = find_app_root_pid(proc.pid);
        }

        if (app_id.empty()) {
            app_id = !proc.name.empty() ? proc.name : ("pid-" + std::to_string(proc.pid));
        }

        auto& bucket = groups[app_id];
        if (bucket.pids.empty()) {
            bucket.app_id = app_id;
            bucket.root_pid = root_pid;

            // Step c: Match against .desktop files
            const DesktopEntry* entry = desktop_db_.find_entry(app_id);
            if (!entry && proc_map.count(root_pid)) {
                const auto& rproc = proc_map.at(root_pid);
                if (!rproc.exe_path.empty()) {
                    std::filesystem::path p(rproc.exe_path);
                    entry = desktop_db_.find_entry(p.filename().string());
                }
                if (!entry) {
                    entry = desktop_db_.find_entry(rproc.name);
                }
            }

            if (entry) {
                bucket.display_name = entry->name;
                bucket.icon_name = entry->icon;
            } else {
                // Step d: Fallback to comm / friendly formatted name
                std::string friendly = app_id;
                // Capitalize first letter if lowercase
                if (!friendly.empty() && friendly[0] >= 'a' && friendly[0] <= 'z') {
                    friendly[0] = static_cast<char>(std::toupper(friendly[0]));
                }
                bucket.display_name = friendly;
                bucket.icon_name = "application-x-executable";
            }
        }

        bucket.pids.push_back(proc.pid);
    }

    // Now construct the final ApplicationGroup list with aggregate metrics & process trees
    std::vector<ApplicationGroup> result;
    result.reserve(groups.size());

    for (auto& [id, bucket] : groups) {
        ApplicationGroup app;
        app.app_id = bucket.app_id;
        app.display_name = bucket.display_name;
        app.icon_name = bucket.icon_name;
        app.root_pid = bucket.root_pid;
        app.process_pids = std::move(bucket.pids);
        app.process_count = static_cast<int>(app.process_pids.size());

        // Calculate aggregate metrics
        for (int pid : app.process_pids) {
            auto it = proc_map.find(pid);
            if (it != proc_map.end()) {
                const auto& p = it->second;
                app.total_cpu_percent += p.cpu_percent;
                app.total_memory_rss_bytes += p.memory_rss_bytes;
                app.total_memory_swap_bytes += p.memory_swap_bytes;
                app.total_threads += p.num_threads;
            }
        }

        // Build hierarchical process trees
        app.root_process_trees = build_process_trees(app.process_pids, proc_map);

        result.push_back(std::move(app));
    }

    // Sort applications by CPU usage descending by default
    std::sort(result.begin(), result.end(), [](const ApplicationGroup& a, const ApplicationGroup& b) {
        if (a.total_cpu_percent != b.total_cpu_percent) {
            return a.total_cpu_percent > b.total_cpu_percent;
        }
        return a.total_memory_rss_bytes > b.total_memory_rss_bytes;
    });

    return result;
}

std::vector<std::shared_ptr<ProcessNode>> AppGrouper::build_process_trees(
    const std::vector<int>& pids,
    const std::unordered_map<int, ProcessInfo>& proc_map) {
    
    std::unordered_map<int, std::shared_ptr<ProcessNode>> node_map;
    node_map.reserve(pids.size());

    for (int pid : pids) {
        auto it = proc_map.find(pid);
        if (it != proc_map.end()) {
            auto node = std::make_shared<ProcessNode>();
            node->info = it->second;
            node_map[pid] = node;
        }
    }

    std::vector<std::shared_ptr<ProcessNode>> root_nodes;

    for (int pid : pids) {
        auto it = node_map.find(pid);
        if (it == node_map.end()) continue;

        auto node = it->second;
        int ppid = node->info.ppid;

        // If parent is also in our group's node_map, link it as child
        auto p_it = node_map.find(ppid);
        if (p_it != node_map.end() && ppid != pid) {
            p_it->second->children.push_back(node);
            node->parent = p_it->second;
        } else {
            // Otherwise, this node acts as a root within this application group
            root_nodes.push_back(node);
        }
    }

    return root_nodes;
}

} // namespace pulse::core
