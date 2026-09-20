#pragma once

#include "pulse/core/rule_engine.hpp"
#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <vector>

namespace pulse::ui {

class RecommendationsPanel : public QWidget {
    Q_OBJECT

public:
    explicit RecommendationsPanel(QWidget* parent = nullptr);

    void update_recommendations(const std::vector<pulse::core::Recommendation>& recs);

signals:
    void dismiss_requested(const QString& rec_id);

private:
    QVBoxLayout* list_layout_{nullptr};
    QLabel* empty_label_{nullptr};
    std::vector<pulse::core::Recommendation> current_recs_;

    void rebuild_ui();
};

} // namespace pulse::ui
