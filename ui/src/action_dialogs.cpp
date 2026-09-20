#include "pulse/ui/action_dialogs.hpp"
#include <QMessageBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>

namespace pulse::ui {

bool ActionDialogs::confirm_force_close(QWidget* parent, const QString& target_name, int pid) {
    QMessageBox msgBox(parent);
    msgBox.setWindowTitle("Force Close Application");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText(QString("Force terminate \"%1\" (PID %2)?").arg(target_name).arg(pid));
    msgBox.setInformativeText("Sending SIGKILL will immediately stop all processes without saving work. Any unsaved changes will be lost.");
    
    QPushButton* killBtn = msgBox.addButton("Force Close (SIGKILL)", QMessageBox::DestructiveRole);
    killBtn->setObjectName("dangerButton");
    msgBox.addButton("Cancel", QMessageBox::RejectRole);
    msgBox.setDefaultButton(QMessageBox::Cancel);

    msgBox.exec();
    return msgBox.clickedButton() == killBtn;
}

void ActionDialogs::execute_graceful_app_close(
    QWidget* parent,
    const pulse::core::ApplicationGroup& app,
    std::shared_ptr<pulse::core::ProcessManager> manager) {

    if (!manager) return;

    // Create graceful closing progress dialog
    auto* dialog = new QDialog(parent);
    dialog->setWindowTitle("Closing " + QString::fromStdString(app.display_name));
    dialog->setFixedSize(420, 200);
    dialog->setStyleSheet("background-color: #101416; color: #FFFFFF;");

    auto* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(14);

    auto* title_lbl = new QLabel("Terminating Application Tree (Children First)...", dialog);
    title_lbl->setFont(QFont("Segoe UI", 11, QFont::Bold));
    layout->addWidget(title_lbl);

    auto* status_lbl = new QLabel(
        QString("Sending SIGTERM to %1 process(es) with a 5-second grace timeout.")
            .arg(app.process_count), dialog);
    status_lbl->setWordWrap(true);
    status_lbl->setStyleSheet("color: #9AA3AF;");
    layout->addWidget(status_lbl);

    auto* pbar = new QProgressBar(dialog);
    pbar->setRange(0, 50); // 50 * 100ms = 5000ms
    pbar->setValue(50);
    pbar->setTextVisible(false);
    pbar->setStyleSheet(
        "QProgressBar { background: #161B1E; border: 1px solid #2A3036; border-radius: 4px; height: 10px; }"
        "QProgressBar::chunk { background: #C6F135; border-radius: 4px; }");
    layout->addWidget(pbar);

    auto* btn_layout = new QHBoxLayout();
    btn_layout->addStretch();

    auto* force_btn = new QPushButton("Escalate to SIGKILL Now", dialog);
    force_btn->setObjectName("dangerButton");
    btn_layout->addWidget(force_btn);

    auto* cancel_btn = new QPushButton("Cancel", dialog);
    btn_layout->addWidget(cancel_btn);

    layout->addLayout(btn_layout);

    // Initial signal dispatch (SIGTERM)
    auto report = manager->close_entire_application(app, false, std::chrono::milliseconds(0));

    auto* timer = new QTimer(dialog);
    int remaining_ticks = 50;

    QObject::connect(timer, &QTimer::timeout, dialog, [dialog, pbar, status_lbl, &remaining_ticks, timer]() {
        remaining_ticks--;
        pbar->setValue(remaining_ticks);
        double secs = remaining_ticks * 0.1;
        status_lbl->setText(QString::asprintf("Waiting for processes to finish saving state... (%.1fs remaining)", secs));

        if (remaining_ticks <= 0) {
            timer->stop();
            status_lbl->setText("Grace period expired. Some processes may still be running.");
            pbar->setStyleSheet("QProgressBar::chunk { background: #EF4444; border-radius: 4px; }");
        }
    });

    QObject::connect(force_btn, &QPushButton::clicked, dialog, [dialog, manager, app]() {
        manager->close_entire_application(app, true, std::chrono::milliseconds(0));
        dialog->accept();
    });

    QObject::connect(cancel_btn, &QPushButton::clicked, dialog, &QDialog::reject);

    timer->start(100);
    dialog->exec();
}

void ActionDialogs::show_termination_report(
    QWidget* parent,
    const QString& action_title,
    const pulse::core::TerminationReport& report) {

    QMessageBox box(parent);
    box.setWindowTitle(action_title);

    switch (report.overall_status) {
        case pulse::core::TerminationStatus::Success:
            box.setIcon(QMessageBox::Information);
            box.setText("Operation Completed Successfully");
            box.setInformativeText(report.summary_message.empty()
                ? "Process signal was dispatched."
                : QString::fromStdString(report.summary_message));
            break;

        case pulse::core::TerminationStatus::ProtectedPid1:
            box.setIcon(QMessageBox::Critical);
            box.setText("Action Denied: System Init (PID 1)");
            box.setInformativeText("PID 1 is the kernel init / systemd daemon. Terminating it would crash the operating system.");
            break;

        case pulse::core::TerminationStatus::ProtectedKernelThread:
            box.setIcon(QMessageBox::Warning);
            box.setText("Action Denied: Kernel Thread");
            box.setInformativeText("Kernel threads (such as kthreadd) run inside the Linux kernel and cannot receive POSIX user-space signals.");
            break;

        case pulse::core::TerminationStatus::ProtectedSelf:
            box.setIcon(QMessageBox::Warning);
            box.setText("Action Denied: Pulse Process");
            box.setInformativeText("Pulse cannot signal its own process from this interface.");
            break;

        case pulse::core::TerminationStatus::PermissionDenied:
            box.setIcon(QMessageBox::Critical);
            box.setText("Permission Denied (EPERM)");
            box.setInformativeText("You do not have permission to signal this process. It is owned by another user or system service. Run Pulse with elevated privileges (sudo/polkit) to manage it.");
            break;

        case pulse::core::TerminationStatus::ProcessNotFound:
            box.setIcon(QMessageBox::Information);
            box.setText("Process Already Terminated (ESRCH)");
            box.setInformativeText("The target process has already exited.");
            break;

        case pulse::core::TerminationStatus::PidReused:
            box.setIcon(QMessageBox::Critical);
            box.setText("Safety Block: PID Reuse Detected");
            box.setInformativeText("The target PID's start timestamp changed before signal dispatch. To prevent terminating an unrelated new process that recycled this PID, the signal was safely canceled.");
            break;

        case pulse::core::TerminationStatus::GraceTimeoutExpired:
            box.setIcon(QMessageBox::Warning);
            box.setText("Grace Period Expired");
            box.setInformativeText(QString::fromStdString(report.summary_message));
            break;

        default:
            box.setIcon(QMessageBox::Warning);
            box.setText("Operation Result");
            box.setInformativeText(QString::fromStdString(report.summary_message));
            break;
    }

    box.exec();
}

} // namespace pulse::ui
