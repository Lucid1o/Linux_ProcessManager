#include "pulse/ui/sparkline_widget.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <algorithm>
#include <cmath>

namespace pulse::ui {

SparklineWidget::SparklineWidget(QWidget* parent)
    : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setMinimumHeight(130);
    setMinimumWidth(200);
}

void SparklineWidget::add_value(double val) {
    history_.push_back(val);
    if (history_.size() > max_points_) {
        history_.pop_front();
    }
    update();
}

void SparklineWidget::set_title(const QString& title) {
    title_ = title;
    update();
}

void SparklineWidget::set_current_value_text(const QString& text) {
    current_value_text_ = text;
    update();
}

void SparklineWidget::set_subtext(const QString& subtext) {
    subtext_ = subtext;
    update();
}

void SparklineWidget::set_fixed_max(double max_val) {
    fixed_max_ = max_val;
    auto_scale_ = (max_val <= 0.0);
    update();
}

void SparklineWidget::set_accent_color(const QColor& line_color, const QColor& fill_color) {
    line_color_ = line_color;
    fill_color_ = fill_color;
    update();
}

void SparklineWidget::set_core_usages(const QVector<double>& core_pcts) {
    core_usages_ = core_pcts;
    update();
}

void SparklineWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    int w = width();
    int h = height();

    // 1. Draw rounded card background and border
    QRectF card_rect(0.5, 0.5, w - 1.0, h - 1.0);
    painter.setPen(QPen(QColor(0x2A, 0x30, 0x36), 1));
    painter.setBrush(QColor(0x10, 0x14, 0x16));
    painter.drawRoundedRect(card_rect, 8.0, 8.0);

    // 2. Draw Title and Current Value
    painter.setFont(QFont("Segoe UI", 9, QFont::Medium));
    painter.setPen(QColor(0x9A, 0xA3, 0xAF)); // Muted text
    painter.drawText(QRectF(14, 12, w - 28, 18), Qt::AlignLeft | Qt::AlignVCenter, title_);

    painter.setFont(QFont("Segoe UI", 16, QFont::Bold));
    painter.setPen(line_color_);
    painter.drawText(QRectF(14, 28, w - 28, 26), Qt::AlignLeft | Qt::AlignVCenter, current_value_text_);

    if (!subtext_.isEmpty()) {
        painter.setFont(QFont("Segoe UI", 8));
        painter.setPen(QColor(0x9A, 0xA3, 0xAF));
        painter.drawText(QRectF(14, 52, w - 28, 16), Qt::AlignLeft | Qt::AlignVCenter, subtext_);
    }

    // Graph bounds
    double graph_left = 14.0;
    double graph_right = w - 14.0;
    double graph_top = subtext_.isEmpty() ? 56.0 : 70.0;
    double graph_bottom = !core_usages_.isEmpty() ? (h - 26.0) : (h - 12.0);
    double graph_height = graph_bottom - graph_top;
    double graph_width = graph_right - graph_left;

    if (graph_height <= 10.0 || graph_width <= 10.0) return;

    // 3. Grid baseline lines
    painter.setPen(QPen(QColor(0x20, 0x27, 0x2D), 1, Qt::DashLine));
    painter.drawLine(QPointF(graph_left, graph_top), QPointF(graph_right, graph_top));
    painter.drawLine(QPointF(graph_left, graph_top + graph_height * 0.5), QPointF(graph_right, graph_top + graph_height * 0.5));
    painter.drawLine(QPointF(graph_left, graph_bottom), QPointF(graph_right, graph_bottom));

    // 4. Sparkline Path & Gradient Area
    if (history_.size() >= 2) {
        double max_val = fixed_max_;
        if (auto_scale_ || max_val <= 0.0) {
            max_val = 1.0;
            for (double v : history_) {
                if (v > max_val) max_val = v;
            }
            max_val *= 1.15; // 15% headroom
        }

        int count = history_.size();
        double step_x = graph_width / (max_points_ - 1);
        double start_x = graph_right - (count - 1) * step_x;

        QPainterPath line_path;
        QPainterPath area_path;

        QPointF first_pt(start_x, graph_bottom - (std::clamp(history_[0] / max_val, 0.0, 1.0) * graph_height));
        line_path.moveTo(first_pt);
        area_path.moveTo(QPointF(start_x, graph_bottom));
        area_path.lineTo(first_pt);

        for (int i = 1; i < count; ++i) {
            double x = start_x + i * step_x;
            double norm = std::clamp(history_[i] / max_val, 0.0, 1.0);
            double y = graph_bottom - (norm * graph_height);
            line_path.lineTo(QPointF(x, y));
            area_path.lineTo(QPointF(x, y));
        }

        area_path.lineTo(QPointF(start_x + (count - 1) * step_x, graph_bottom));
        area_path.closeSubpath();

        // Area Gradient
        QLinearGradient grad(0, graph_top, 0, graph_bottom);
        grad.setColorAt(0.0, fill_color_);
        grad.setColorAt(1.0, QColor(fill_color_.red(), fill_color_.green(), fill_color_.blue(), 0));

        painter.fillPath(area_path, grad);

        // Outline Line
        painter.setPen(QPen(line_color_, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(line_path);

        // Highlight latest point
        double last_x = start_x + (count - 1) * step_x;
        double last_y = graph_bottom - (std::clamp(history_.back() / max_val, 0.0, 1.0) * graph_height);
        painter.setBrush(line_color_);
        painter.setPen(QColor(0x08, 0x0A, 0x0A));
        painter.drawEllipse(QPointF(last_x, last_y), 3.5, 3.5);
    }

    // 5. Per-Core CPU Mini Bars (if present)
    if (!core_usages_.isEmpty()) {
        int n_cores = core_usages_.size();
        double bar_y = h - 18.0;
        double bar_h = 8.0;
        double total_spacing = 3.0 * (n_cores - 1);
        double bar_w = (graph_width - total_spacing) / n_cores;

        for (int i = 0; i < n_cores; ++i) {
            double bx = graph_left + i * (bar_w + 3.0);
            QRectF bg_bar(bx, bar_y, bar_w, bar_h);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0x1F, 0x24, 0x28));
            painter.drawRoundedRect(bg_bar, 2.0, 2.0);

            double core_pct = std::clamp(core_usages_[i] / 100.0, 0.0, 1.0);
            QRectF fill_bar(bx, bar_y, bar_w * core_pct, bar_h);
            QColor core_col = (core_pct > 0.8) ? QColor(0xEF, 0x44, 0x44) : line_color_;
            painter.setBrush(core_col);
            painter.drawRoundedRect(fill_bar, 2.0, 2.0);
        }
    }
}

} // namespace pulse::ui
