#pragma once

#include <QWidget>
#include <QVector>
#include <QColor>
#include <QString>

namespace pulse::ui {

class SparklineWidget : public QWidget {
    Q_OBJECT

public:
    explicit SparklineWidget(QWidget* parent = nullptr);

    void add_value(double val);
    void set_title(const QString& title);
    void set_current_value_text(const QString& text);
    void set_subtext(const QString& subtext);
    void set_fixed_max(double max_val); // e.g. 100.0 for percent
    void set_accent_color(const QColor& line_color, const QColor& fill_color);
    void set_core_usages(const QVector<double>& core_pcts); // For CPU per-core mini bars

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString title_;
    QString current_value_text_;
    QString subtext_;
    QVector<double> history_;
    int max_points_{60};
    double fixed_max_{100.0};
    bool auto_scale_{false};
    QColor line_color_{0xC6, 0xF1, 0x35}; // Lime accent default
    QColor fill_color_{0xC6, 0xF1, 0x35, 40};
    QVector<double> core_usages_;
};

} // namespace pulse::ui
