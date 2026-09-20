#include "pulse/core/app_grouper.hpp"
#include <cassert>
#include <iostream>

using namespace pulse::core;

void test_cgroup_extraction() {
    std::string cg1 = "0::/user.slice/user-1000.slice/user@1000.service/app.slice/app-google\\x2dchrome-500.scope\n";
    std::string app1 = AppGrouper::extract_cgroup_app_scope(cg1);
    assert(app1 == "google-chrome");

    std::string cg2 = "0::/user.slice/user-1000.slice/user@1000.service/app.slice/app-flatpak-org.mozilla.firefox-1234.scope\n";
    std::string app2 = AppGrouper::extract_cgroup_app_scope(cg2);
    assert(app2 == "org.mozilla.firefox");

    std::string cg3 = "0::/system.slice/sshd.service\n";
    std::string app3 = AppGrouper::extract_cgroup_app_scope(cg3);
    assert(app3 == ""); // Not an app scope

    std::cout << "[PASS] test_cgroup_extraction\n";
}

void test_desktop_database() {
    DesktopEntryDatabase db;
    DesktopEntry entry;
    entry.id = "google-chrome";
    entry.name = "Google Chrome";
    entry.exec_name = "google-chrome-stable";
    entry.icon = "google-chrome";
    db.add_entry(entry);

    const DesktopEntry* found_id = db.find_entry("google-chrome");
    assert(found_id != nullptr);
    assert(found_id->name == "Google Chrome");

    const DesktopEntry* found_exec = db.find_entry("google-chrome-stable");
    assert(found_exec != nullptr);
    assert(found_exec->name == "Google Chrome");

    std::cout << "[PASS] test_desktop_database\n";
}

void test_grouping_and_tree() {
    DesktopEntryDatabase db;
    DesktopEntry chrome_entry;
    chrome_entry.id = "google-chrome";
    chrome_entry.name = "Google Chrome";
    chrome_entry.exec_name = "google-chrome";
    chrome_entry.icon = "google-chrome";
    db.add_entry(chrome_entry);

    AppGrouper grouper(db);

    // Create a tree of Chrome processes:
    // Root browser (PID 500)
    //   -> Renderer (PID 501)
    //        -> Worker (PID 502)
    std::vector<ProcessInfo> procs;

    ProcessInfo p500;
    p500.pid = 500;
    p500.ppid = 1;
    p500.name = "chrome";
    p500.cpu_percent = 5.0;
    p500.memory_rss_bytes = 100 * 1024 * 1024;
    p500.cgroup = "0::/user.slice/user-1000.slice/user@1000.service/app.slice/app-google\\x2dchrome-500.scope";
    procs.push_back(p500);

    ProcessInfo p501;
    p501.pid = 501;
    p501.ppid = 500;
    p501.name = "chrome";
    p501.cpu_percent = 10.0;
    p501.memory_rss_bytes = 150 * 1024 * 1024;
    p501.cgroup = p500.cgroup;
    procs.push_back(p501);

    ProcessInfo p502;
    p502.pid = 502;
    p502.ppid = 501;
    p502.name = "chrome";
    p502.cpu_percent = 2.0;
    p502.memory_rss_bytes = 50 * 1024 * 1024;
    p502.cgroup = p500.cgroup;
    procs.push_back(p502);

    auto groups = grouper.group_processes(procs);
    assert(groups.size() == 1);

    const auto& app = groups[0];
    assert(app.app_id == "google-chrome");
    assert(app.display_name == "Google Chrome");
    assert(app.process_count == 3);
    assert(app.total_cpu_percent == 17.0);
    assert(app.total_memory_rss_bytes == 300 * 1024 * 1024);

    // Verify process tree structure
    assert(app.root_process_trees.size() == 1);
    auto root_node = app.root_process_trees[0];
    assert(root_node->info.pid == 500);
    assert(root_node->children.size() == 1);
    assert(root_node->children[0]->info.pid == 501);
    assert(root_node->children[0]->children.size() == 1);
    assert(root_node->children[0]->children[0]->info.pid == 502);

    std::cout << "[PASS] test_grouping_and_tree\n";
}

int main() {
    test_cgroup_extraction();
    test_desktop_database();
    test_grouping_and_tree();
    std::cout << "All grouper tests passed successfully!\n";
    return 0;
}
