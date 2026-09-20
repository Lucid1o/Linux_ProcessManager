#include "pulse/core/rule_engine.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>

namespace pulse::core {

RuleEngine::RuleEngine() {
    load_default_rules();
}

void RuleEngine::load_default_rules() {
    rules_.clear();

    // Rule 1: High RAM duration
    RuleDefinition ram_rule;
    ram_rule.id = "high_memory_duration";
    ram_rule.name = "High Memory Consumption";
    ram_rule.condition_type = ConditionType::MemoryPercent;
    ram_rule.threshold = 25.0; // 25% of total RAM
    ram_rule.duration_seconds = 300; // 5 minutes (300 seconds)
    ram_rule.severity = RuleSeverity::Warning;
    ram_rule.message_template = "{app} has been using high memory ({value}%) for an extended period.";
    ram_rule.suggestion = "Consider closing unused tabs or restarting the application to free up memory.";
    rules_.push_back(ram_rule);

    // Rule 2: High CPU duration
    RuleDefinition cpu_rule;
    cpu_rule.id = "high_cpu_duration";
    cpu_rule.name = "Extended High CPU Usage";
    cpu_rule.condition_type = ConditionType::CpuPercent;
    cpu_rule.threshold = 80.0; // 80% CPU
    cpu_rule.duration_seconds = 120; // 2 minutes (120 seconds)
    cpu_rule.severity = RuleSeverity::Warning;
    cpu_rule.message_template = "{app} has been consuming high CPU ({value}%) for over {duration}.";
    cpu_rule.suggestion = "Review active tasks or background processes within this application.";
    rules_.push_back(cpu_rule);

    // Rule 3: Zombie process detection
    RuleDefinition zombie_rule;
    zombie_rule.id = "zombie_process_detected";
    zombie_rule.name = "Defunct Zombie Process";
    zombie_rule.condition_type = ConditionType::ProcessState;
    zombie_rule.target_state = "Z";
    zombie_rule.threshold = 0.0;
    zombie_rule.duration_seconds = 0; // Immediate detection
    zombie_rule.severity = RuleSeverity::Critical;
    zombie_rule.message_template = "Zombie process detected: {app} (PID {pid}) is defunct and cannot be reclaimed by its parent.";
    zombie_rule.suggestion = "Inspect parent process health or terminate the parent process to clean up zombie slots.";
    rules_.push_back(zombie_rule);
}

void RuleEngine::add_rule(const RuleDefinition& rule) {
    // Replace if exists, else append
    for (auto& r : rules_) {
        if (r.id == rule.id) {
            r = rule;
            return;
        }
    }
    rules_.push_back(rule);
}

// Lightweight JSON parser for rules array
bool RuleEngine::load_rules_from_json(const std::string& json_content) {
    // Parse JSON array of objects:
    // [
    //   { "id": "...", "name": "...", "condition": "memory_percent", "threshold": 25.0, ... }
    // ]
    try {
        size_t pos = json_content.find('[');
        if (pos == std::string::npos) return false;

        std::vector<RuleDefinition> parsed_rules;

        while (pos < json_content.size()) {
            size_t obj_start = json_content.find('{', pos);
            if (obj_start == std::string::npos) break;
            size_t obj_end = json_content.find('}', obj_start);
            if (obj_end == std::string::npos) break;

            std::string obj_str = json_content.substr(obj_start, obj_end - obj_start + 1);
            pos = obj_end + 1;

            RuleDefinition r;
            auto get_string = [&](const std::string& key) -> std::string {
                std::string search = "\"" + key + "\"";
                auto kpos = obj_str.find(search);
                if (kpos == std::string::npos) return "";
                auto colon = obj_str.find(':', kpos + search.size());
                if (colon == std::string::npos) return "";
                auto q1 = obj_str.find('\"', colon + 1);
                if (q1 == std::string::npos) return "";
                auto q2 = obj_str.find('\"', q1 + 1);
                if (q2 == std::string::npos) return "";
                return obj_str.substr(q1 + 1, q2 - q1 - 1);
            };

            auto get_number = [&](const std::string& key, double default_val) -> double {
                std::string search = "\"" + key + "\"";
                auto kpos = obj_str.find(search);
                if (kpos == std::string::npos) return default_val;
                auto colon = obj_str.find(':', kpos + search.size());
                if (colon == std::string::npos) return default_val;
                size_t num_start = obj_str.find_first_of("0123456789.-+", colon + 1);
                if (num_start == std::string::npos) return default_val;
                size_t num_end = obj_str.find_first_not_of("0123456789.eE-+", num_start);
                std::string val_str = obj_str.substr(num_start, num_end - num_start);
                try {
                    return std::stod(val_str);
                } catch (...) {
                    return default_val;
                }
            };

            r.id = get_string("id");
            r.name = get_string("name");
            std::string cond_str = get_string("condition_type");
            if (cond_str.empty()) cond_str = get_string("condition");

            if (cond_str == "memory_percent") {
                r.condition_type = ConditionType::MemoryPercent;
            } else if (cond_str == "cpu_percent") {
                r.condition_type = ConditionType::CpuPercent;
            } else if (cond_str == "process_state") {
                r.condition_type = ConditionType::ProcessState;
            } else {
                continue; // Unknown condition
            }

            r.threshold = get_number("threshold", 0.0);
            r.duration_seconds = static_cast<int>(get_number("duration_seconds", 0.0));
            r.target_state = get_string("target_state");
            if (r.target_state.empty()) r.target_state = "Z";

            std::string sev_str = get_string("severity");
            r.severity = string_to_rule_severity(sev_str);

            r.message_template = get_string("message_template");
            r.suggestion = get_string("suggestion");

            if (!r.id.empty()) {
                parsed_rules.push_back(r);
            }
        }

        if (!parsed_rules.empty()) {
            for (const auto& r : parsed_rules) {
                add_rule(r);
            }
            return true;
        }
    } catch (...) {
        return false;
    }
    return false;
}

bool RuleEngine::load_rules_from_file(const std::string& json_path) {
    std::ifstream f(json_path);
    if (!f.is_open()) return false;
    std::stringstream buffer;
    buffer << f.rdbuf();
    return load_rules_from_json(buffer.str());
}

std::string RuleEngine::format_message(const std::string& tmpl,
                                       const std::string& app_name,
                                       double value,
                                       int duration_sec,
                                       int pid) {
    std::string out = tmpl;

    auto replace_all = [&](const std::string& placeholder, const std::string& rep) {
        size_t idx = 0;
        while ((idx = out.find(placeholder, idx)) != std::string::npos) {
            out.replace(idx, placeholder.size(), rep);
            idx += rep.size();
        }
    };

    replace_all("{app}", app_name);
    replace_all("{process}", app_name);

    std::ostringstream val_ss;
    val_ss << std::fixed << std::setprecision(1) << value;
    replace_all("{value}", val_ss.str());

    std::string dur_str;
    if (duration_sec >= 60) {
        dur_str = std::to_string(duration_sec / 60) + "m";
    } else {
        dur_str = std::to_string(duration_sec) + "s";
    }
    replace_all("{duration}", dur_str);
    replace_all("{pid}", std::to_string(pid));

    return out;
}

void RuleEngine::dismiss_recommendation(const std::string& rec_id) {
    dismissed_ids_.insert(rec_id);
    for (auto& r : active_recommendations_) {
        if (r.id == rec_id) {
            r.dismissed = true;
        }
    }
}

bool RuleEngine::is_dismissed(const std::string& rec_id) const {
    return dismissed_ids_.count(rec_id) > 0;
}

void RuleEngine::clear_dismissed() {
    dismissed_ids_.clear();
}

std::vector<Recommendation> RuleEngine::evaluate(
    const std::vector<ApplicationGroup>& apps,
    const std::vector<ProcessInfo>& all_procs,
    const SystemMetrics& metrics,
    std::chrono::steady_clock::time_point current_time) {

    std::vector<Recommendation> results;
    std::unordered_set<std::string> active_keys_this_tick;

    for (const auto& rule : rules_) {
        if (rule.condition_type == ConditionType::MemoryPercent) {
            double total_ram_mb = (metrics.mem_total_bytes > 0)
                ? (static_cast<double>(metrics.mem_total_bytes) / (1024.0 * 1024.0))
                : 1.0;

            for (const auto& app : apps) {
                double app_ram_mb = static_cast<double>(app.total_memory_rss_bytes) / (1024.0 * 1024.0);
                double ram_pct = (app_ram_mb / total_ram_mb) * 100.0;

                std::string key = rule.id + ":" + app.app_id;
                if (ram_pct >= rule.threshold) {
                    active_keys_this_tick.insert(key);
                    auto& tracker = active_trackers_[key];
                    if (tracker.first_met_time.time_since_epoch().count() == 0) {
                        tracker.first_met_time = current_time;
                    }
                    tracker.last_seen_time = current_time;

                    auto elapsed_sec = std::chrono::duration_cast<std::chrono::seconds>(
                        current_time - tracker.first_met_time).count();

                    if (elapsed_sec >= rule.duration_seconds) {
                        tracker.triggered = true;
                        if (!is_dismissed(key)) {
                            Recommendation rec;
                            rec.id = key;
                            rec.rule_id = rule.id;
                            rec.entity_id = app.app_id;
                            rec.app_display_name = app.display_name;
                            rec.severity = rule.severity;
                            rec.title = rule.name + ": " + app.display_name;
                            rec.message = format_message(rule.message_template, app.display_name, ram_pct, static_cast<int>(elapsed_sec), app.root_pid);
                            rec.suggestion = rule.suggestion;
                            rec.timestamp = std::chrono::system_clock::now();
                            results.push_back(rec);
                        }
                    }
                }
            }
        } else if (rule.condition_type == ConditionType::CpuPercent) {
            for (const auto& app : apps) {
                double cpu_pct = app.total_cpu_percent;
                std::string key = rule.id + ":" + app.app_id;

                if (cpu_pct >= rule.threshold) {
                    active_keys_this_tick.insert(key);
                    auto& tracker = active_trackers_[key];
                    if (tracker.first_met_time.time_since_epoch().count() == 0) {
                        tracker.first_met_time = current_time;
                    }
                    tracker.last_seen_time = current_time;

                    auto elapsed_sec = std::chrono::duration_cast<std::chrono::seconds>(
                        current_time - tracker.first_met_time).count();

                    if (elapsed_sec >= rule.duration_seconds) {
                        tracker.triggered = true;
                        if (!is_dismissed(key)) {
                            Recommendation rec;
                            rec.id = key;
                            rec.rule_id = rule.id;
                            rec.entity_id = app.app_id;
                            rec.app_display_name = app.display_name;
                            rec.severity = rule.severity;
                            rec.title = rule.name + ": " + app.display_name;
                            rec.message = format_message(rule.message_template, app.display_name, cpu_pct, static_cast<int>(elapsed_sec), app.root_pid);
                            rec.suggestion = rule.suggestion;
                            rec.timestamp = std::chrono::system_clock::now();
                            results.push_back(rec);
                        }
                    }
                }
            }
        } else if (rule.condition_type == ConditionType::ProcessState) {
            char target_char = rule.target_state.empty() ? 'Z' : rule.target_state[0];
            for (const auto& proc : all_procs) {
                if (static_cast<char>(proc.state) == target_char) {
                    std::string key = rule.id + ":pid_" + std::to_string(proc.pid);
                    active_keys_this_tick.insert(key);

                    auto& tracker = active_trackers_[key];
                    if (tracker.first_met_time.time_since_epoch().count() == 0) {
                        tracker.first_met_time = current_time;
                    }
                    tracker.last_seen_time = current_time;

                    auto elapsed_sec = std::chrono::duration_cast<std::chrono::seconds>(
                        current_time - tracker.first_met_time).count();

                    if (elapsed_sec >= rule.duration_seconds) {
                        tracker.triggered = true;
                        if (!is_dismissed(key)) {
                            Recommendation rec;
                            rec.id = key;
                            rec.rule_id = rule.id;
                            rec.entity_id = std::to_string(proc.pid);
                            rec.app_display_name = proc.name;
                            rec.severity = rule.severity;
                            rec.title = rule.name + " (PID " + std::to_string(proc.pid) + ")";
                            rec.message = format_message(rule.message_template, proc.name, 0.0, static_cast<int>(elapsed_sec), proc.pid);
                            rec.suggestion = rule.suggestion;
                            rec.timestamp = std::chrono::system_clock::now();
                            results.push_back(rec);
                        }
                    }
                }
            }
        }
    }

    // Clean up trackers for conditions that are no longer met
    for (auto it = active_trackers_.begin(); it != active_trackers_.end(); ) {
        if (active_keys_this_tick.find(it->first) == active_keys_this_tick.end()) {
            it = active_trackers_.erase(it);
        } else {
            ++it;
        }
    }

    active_recommendations_ = results;
    return results;
}

} // namespace pulse::core
