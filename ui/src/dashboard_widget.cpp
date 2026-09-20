#include "pulse/ui/dashboard_widget.hpp"
#include <QHBoxLayout>
#include <iomanip>
#include <sstream>

namespace pulse::ui {

DashboardWidget::DashboardWidget(QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // 1. CPU Card (Lime Accent #C6F135)
    cpu_sparkline_ = new SparklineWidget(this);
    cpu_sparkline_->set_title("CPU UTILIZATION");
    cpu_sparkline_->set_current_value_text("0.0%");
    cpu_sparkline_->set_fixed_max(100.0);
    cpu_sparkline_->set_accent_color(QColor(0xC6, 0xF1, 0x35), QColor(0xC6, 0xF1, 0x35, 45));
    layout->addWidget(cpu_sparkline_);

    // 2. Memory Card (Purple Accent #7C3AED)
    mem_sparkline_ = new SparklineWidget(this);
    mem_sparkline_->set_title("MEMORY & SWAP");
    mem_sparkline_->set_current_value_text("0.0 GB");
    mem_sparkline_->set_fixed_max(100.0);
    mem_sparkline_->set_accent_color(QColor(0x7C, 0x3A, 0xED), QColor(0x7C, 0x3A, 0xED, 50));
    layout->addWidget(mem_sparkline_);

    // 3. Disk I/O Card (Cyan Accent #38BDF8)
    disk_sparkline_ = new SparklineWidget(this);
    disk_sparkline_->set_title("DISK THROUGHPUT");
    disk_sparkline_->set_current_value_text("0.0 KB/s");
    disk_sparkline_->set_fixed_max(0.0); // auto-scale
    disk_sparkline_->set_accent_color(QColor(0x38, 0xBD, 0xF8), QColor(0x38, 0xBD, 0xF8, 45));
    layout->addWidget(disk_sparkline_);

    // 4. Network Card (Emerald Accent #34D399)
    net_sparkline_ = new SparklineWidget(this);
    net_sparkline_->set_title("NETWORK TRAFFIC");
    net_sparkline_->set_current_value_text("0.0 KB/s");
    net_sparkline_->set_fixed_max(0.0); // auto-scale
    net_sparkline_->set_accent_color(QColor(0x34, 0xD3, 0x99), QColor(0x34, 0xD3, 0x99, 45));
    layout->addWidget(net_sparkline_);
}

QString DashboardWidget::format_bytes(uint64_t bytes) {
    const double kb = 1024.0;
    const double mb = kb * 1024.0;
    const double gb = mb * 1024.0;
    const double tb = gb * 1024.0;

    double d_bytes = static_cast<double>(bytes);
    if (d_bytes >= tb) {
        return QString::asprintf("%.2f TB", d_bytes / tb);
    } else if (d_bytes >= gb) {
        return QString::asprintf("%.1f GB", d_bytes / gb);
    } else if (d_bytes >= mb) {
        return QString::asprintf("%.1f MB", d_bytes / mb);
    } else if (d_bytes >= kb) {
        return QString::asprintf("%.0f KB", d_bytes / kb);
    } else {
        return QString::asprintf("%llu B", static_cast<unsigned long long>(bytes));
    }
}

QString DashboardWidget::format_rate(uint64_t bytes_per_sec) {
    return format_bytes(bytes_per_sec) + "/s";
}

void DashboardWidget::update_metrics(const pulse::core::SystemMetrics& metrics) {
    // 1. CPU
    cpu_sparkline_->add_value(metrics.total_cpu_percent);
    cpu_sparkline_->set_current_value_text(QString::asprintf("%.1f%%", metrics.total_cpu_percent));
    cpu_sparkline_->set_subtext(QString::asprintf("%d Cores | %d Procs | %d Threads",
        static_cast<int>(metrics.cores.size()), metrics.total_processes, metrics.total_threads));

    QVector<double> core_vals;
    core_vals.reserve(static_cast<int>(metrics.cores.size()));
    for (const auto& c : metrics.cores) {
        core_vals.push_back(c.usage_percent);
    }
    cpu_sparkline_->set_core_usages(core_vals);

    // 2. Memory
    double ram_pct = 0.0;
    if (metrics.mem_total_bytes > 0) {
        ram_pct = (static_cast<double>(metrics.mem_used_bytes) / metrics.mem_total_bytes) * 100.0;
    }
    mem_sparkline_->add_value(ram_pct);
    mem_sparkline_->set_current_value_text(format_bytes(metrics.mem_used_bytes) + " / " + format_bytes(metrics.mem_total_bytes));
    mem_sparkline_->set_subtext(QString("RAM: %1% | Swap: %2 / %3")
        .arg(QString::asprintf("%.1f", ram_pct))
        .arg(format_bytes(metrics.swap_used_bytes))
        .arg(format_bytes(metrics.swap_total_bytes)));

    // 3. Disk
    uint64_t disk_total_rate = metrics.disk.read_bytes_per_sec + metrics.disk.write_bytes_per_sec;
    disk_sparkline_->add_value(static_cast<double>(disk_total_rate));
    disk_sparkline_->set_current_value_text(format_rate(disk_total_rate));
    disk_sparkline_->set_subtext(QString("R: %1 | W: %2")
        .arg(format_rate(metrics.disk.read_bytes_per_sec))
        .arg(format_rate(metrics.disk.write_bytes_per_sec)));

    // 4. Network
    uint64_t net_total_rate = metrics.network.rx_bytes_per_sec + metrics.network.tx_bytes_per_sec;
    net_sparkline_->add_value(static_cast<double>(net_total_rate));
    net_sparkline_->set_current_value_text(format_rate(net_total_rate));
    net_sparkline_->set_subtext(QString("↓ %1 | ↑ %2")
        .arg(format_rate(metrics.network.rx_bytes_per_sec))
        .arg(format_rate(metrics.network.tx_bytes_per_sec)));
}

} // namespace pulse::ui
