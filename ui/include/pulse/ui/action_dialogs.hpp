#pragma once

#include "pulse/core/process_info.hpp"
#include "pulse/core/process_manager.hpp"
#include <QWidget>
#include <QString>
#include <memory>

namespace pulse::ui {

class ActionDialogs {
public:
    // Confirmation dialog before SIGKILL
    static bool confirm_force_close(QWidget* parent, const QString& target_name, int pid);

    // Dialog showing countdown and escalation for Close Entire Application
    static void execute_graceful_app_close(
        QWidget* parent,
        const pulse::core::ApplicationGroup& app,
        std::shared_ptr<pulse::core::ProcessManager> manager);

    // Show result message dialog with friendly error explanations
    static void show_termination_report(
        QWidget* parent,
        const QString& action_title,
        const pulse::core::TerminationReport& report);
};

} // namespace pulse::ui
