#pragma once

#include "pulse/core/process_info.hpp"
#include "pulse/ui/sparkline_widget.hpp"
#include <QWidget>
#include <QHBoxLayout>

namespace pulse::ui {

class DashboardWidget : public QWidget {
    Q_OBJECT

public:
    explicit DashboardWidget(QWidget* parent = nullptr);

    void update_metrics(const pulse::core::SystemMetrics& metrics);

private:
    SparklineWidget* cpu_sparkline_{nullptr};
    SparklineWidget* mem_sparkline_{nullptr};
    SparklineWidget* disk_sparkline_{nullptr};
    SparklineWidget* net_sparkline_{nullptr};

    static QString format_bytes(uint64_t bytes);
    static QString format_rate(uint64_t bytes_per_sec);
};

} // namespace pulse::ui
