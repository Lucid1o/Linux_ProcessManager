#include "pulse/ui/main_window.hpp"
#include "pulse/collector/linux_proc_collector.hpp"
#include "pulse/collector/simulated_collector.hpp"

#include <QApplication>
#include <QFile>
#include <QStyleFactory>
#include <filesystem>
#include <iostream>

#ifdef __linux__
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>
#include <sstream>

class LinuxProcessController : public pulse::core::IProcessController {
public:
    bool get_process_state(int pid, uint64_t& out_start_time, int& out_uid, bool& out_is_kthread) override {
        std::string stat_path = "/proc/" + std::to_string(pid) + "/stat";
        std::ifstream f(stat_path);
        if (!f.is_open()) return false;

        std::string content;
        std::getline(f, content);
        auto close_paren = content.rfind(')');
        if (close_paren == std::string::npos) return false;

        std::istringstream iss(content.substr(close_paren + 1));
        char state;
        int ppid, pgrp, session, tty, tpgid;
        unsigned int flags;
        unsigned long minflt, cminflt, majflt, cmajflt, utime, stime;
        long cutime, cstime, prio, nice, num_thr, itreal;
        unsigned long long starttime;
        if (iss >> state >> ppid >> pgrp >> session >> tty >> tpgid >> flags
                >> minflt >> cminflt >> majflt >> cmajflt >> utime >> stime
                >> cutime >> cstime >> prio >> nice >> num_thr >> itreal >> starttime) {
            out_start_time = starttime;
        }

        // Read Uid from /proc/<pid>/status
        std::string status_path = "/proc/" + std::to_string(pid) + "/status";
        std::ifstream sf(status_path);
        std::string sline;
        while (std::getline(sf, sline)) {
            if (sline.rfind("Uid:", 0) == 0) {
                std::istringstream uiss(sline.substr(4));
                uiss >> out_uid;
                break;
            }
        }

        out_is_kthread = (pid == 2 || ppid == 2);
        return true;
    }

    pulse::core::TerminationStatus send_signal(int pid, pulse::core::ProcessSignal signal) override {
        int sig = static_cast<int>(signal);
        if (::kill(pid, sig) == 0) {
            return pulse::core::TerminationStatus::Success;
        }
        if (errno == EPERM) return pulse::core::TerminationStatus::PermissionDenied;
        if (errno == ESRCH) return pulse::core::TerminationStatus::ProcessNotFound;
        return pulse::core::TerminationStatus::Failed;
    }

    int get_self_pid() const override { return ::getpid(); }
    int get_self_uid() const override { return ::getuid(); }
};
#endif

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Pulse");
    app.setApplicationDisplayName("Pulse Process Manager");
    app.setStyle(QStyleFactory::create("Fusion"));

    // Apply dark stylesheet from resource or fallback file
    QFile qss_file(":/style.qss");
    if (qss_file.open(QFile::ReadOnly | QFile::Text)) {
        app.setStyleSheet(qss_file.readAll());
        qss_file.close();
    } else {
        QFile fallback_qss("resources/style.qss");
        if (fallback_qss.open(QFile::ReadOnly | QFile::Text)) {
            app.setStyleSheet(fallback_qss.readAll());
            fallback_qss.close();
        }
    }

    std::shared_ptr<pulse::collector::ISystemCollector> collector;
    std::shared_ptr<pulse::core::IProcessController> controller;

#ifdef __linux__
    bool has_proc = std::filesystem::exists("/proc") && std::filesystem::is_directory("/proc");
    if (has_proc) {
        std::cout << "[Pulse] Initializing Linux /proc collector and POSIX signal controller.\n";
        collector = std::make_shared<pulse::collector::LinuxProcCollector>("/proc", "/sys");
        controller = std::make_shared<LinuxProcessController>();
    }
#endif

    if (!collector || !controller) {
        std::cout << "[Pulse] Non-Linux or simulation environment detected. Initializing simulated Linux procfs collector.\n";
        auto sim = std::make_shared<pulse::collector::SimulatedCollector>();
        collector = sim;
        controller = sim;
    }

    pulse::ui::MainWindow window(collector, controller);

    // Check for command line flags
    QString screenshot_path;
    bool test_run = false;
    bool start_advanced = false;
    bool expand_all = false;
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--screenshot" && i + 1 < argc) {
            screenshot_path = QString::fromLocal8Bit(argv[++i]);
        } else if (arg == "--test-run") {
            test_run = true;
        } else if (arg == "--advanced") {
            start_advanced = true;
        } else if (arg == "--expand-all") {
            expand_all = true;
        }
    }

    if (start_advanced) {
        QMetaObject::invokeMethod(&window, "on_mode_toggled", Q_ARG(bool, true));
    }
    if (expand_all) {
        // Expand tree
        auto* tree = window.findChild<QTreeView*>();
        if (tree) {
            tree->expandAll();
        }
    }

    window.show();

    if (!screenshot_path.isEmpty()) {
        QTimer::singleShot(1000, [&window, screenshot_path, expand_all]() {
            if (expand_all) {
                auto* tree = window.findChild<QTreeView*>();
                if (tree) tree->expandAll();
            }
            QPixmap pix = window.grab();
            if (pix.save(screenshot_path)) {
                std::cout << "[Pulse] Screenshot saved successfully to: " << screenshot_path.toStdString() << "\n";
            } else {
                std::cerr << "[Pulse] Failed to save screenshot to: " << screenshot_path.toStdString() << "\n";
            }
            qApp->quit();
        });
    } else if (test_run) {
        QTimer::singleShot(1000, []() {
            std::cout << "[Pulse] Test run completed successfully.\n";
            qApp->quit();
        });
    }

    return app.exec();
}
