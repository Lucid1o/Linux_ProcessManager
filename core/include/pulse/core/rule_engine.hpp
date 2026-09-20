#pragma once

#include "pulse/core/process_info.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <optional>

namespace pulse::core {

enum class RuleSeverity {
    Info,
    Warning,
    Critical
};

inline const char* rule_severity_to_string(RuleSeverity s) {
    switch (s) {
        case RuleSeverity::Info: return "info";
        case RuleSeverity::Warning: return "warning";
        case RuleSeverity::Critical: return "critical";
        default: return "info";
    }
}

inline RuleSeverity string_to_rule_severity(const std::string& str) {
    if (str == "warning" || str == "Warning") return RuleSeverity::Warning;
    if (str == "critical" || str == "Critical") return RuleSeverity::Critical;
    return RuleSeverity::Info;
}

enum class ConditionType {
    MemoryPercent,
    CpuPercent,
    ProcessState
};

struct RuleDefinition {
    std::string id;
    std::string name;
    ConditionType condition_type{ConditionType::MemoryPercent};
    double threshold{0.0};                     // e.g. 25.0 for 25% RAM, 80.0 for 80% CPU
    std::string target_state;                  // e.g. "Z" for Zombie
    int duration_seconds{0};                   // 0 = immediate, 300 = 5 minutes, etc.
    RuleSeverity severity{RuleSeverity::Warning};
    std::string message_template;              // Formattable text
    std::string suggestion;                    // Plain language actionable suggestion
};

struct Recommendation {
    std::string id;                            // rule_id + ":" + entity_id
    std::string rule_id;
    std::string entity_id;                     // app_id or pid string
    std::string app_display_name;
    RuleSeverity severity{RuleSeverity::Warning};
    std::string title;
    std::string message;
    std::string suggestion;
    std::chrono::system_clock::time_point timestamp;
    bool dismissed{false};
};

class RuleEngine {
public:
    RuleEngine();

    // Load rules from JSON string or file
    bool load_rules_from_file(const std::string& json_path);
    bool load_rules_from_json(const std::string& json_content);
    void add_rule(const RuleDefinition& rule);

    // Initialize sensible default rules
    void load_default_rules();

    // Evaluate rules against current application groups, processes, and system metrics
    std::vector<Recommendation> evaluate(
        const std::vector<ApplicationGroup>& apps,
        const std::vector<ProcessInfo>& all_procs,
        const SystemMetrics& metrics,
        std::chrono::steady_clock::time_point current_time = std::chrono::steady_clock::now());

    // Dismiss a recommendation by ID
    void dismiss_recommendation(const std::string& rec_id);
    bool is_dismissed(const std::string& rec_id) const;
    void clear_dismissed();

    const std::vector<RuleDefinition>& rules() const { return rules_; }
    const std::vector<Recommendation>& active_recommendations() const { return active_recommendations_; }

private:
    std::vector<RuleDefinition> rules_;
    std::vector<Recommendation> active_recommendations_;
    std::unordered_set<std::string> dismissed_ids_;

    struct ConditionTracker {
        std::chrono::steady_clock::time_point first_met_time;
        std::chrono::steady_clock::time_point last_seen_time;
        bool triggered{false};
    };

    // Key: rule_id + ":" + entity_id
    std::unordered_map<std::string, ConditionTracker> active_trackers_;

    std::string format_message(const std::string& tmpl,
                               const std::string& app_name,
                               double value,
                               int duration_sec,
                               int pid);
};

} // namespace pulse::core
