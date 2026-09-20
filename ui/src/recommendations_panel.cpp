#include "pulse/ui/recommendations_panel.hpp"
#include <QPushButton>
#include <QFrame>
#include <QHBoxLayout>

namespace pulse::ui {

RecommendationsPanel::RecommendationsPanel(QWidget* parent)
    : QWidget(parent) {
    setObjectName("cardWidget");
    setMinimumWidth(280);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(14, 14, 14, 14);
    main_layout->setSpacing(10);

    // Header
    auto* header_layout = new QHBoxLayout();
    auto* title_label = new QLabel("INSIGHTS & RECOMMENDATIONS", this);
    title_label->setFont(QFont("Segoe UI", 10, QFont::Bold));
    title_label->setStyleSheet("color: #C6F135; letter-spacing: 0.5px;");
    header_layout->addWidget(title_label);
    header_layout->addStretch();
    main_layout->addLayout(header_layout);

    // Scroll Area for items
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background: transparent;");

    auto* container = new QWidget(scroll);
    container->setStyleSheet("background: transparent;");
    list_layout_ = new QVBoxLayout(container);
    list_layout_->setContentsMargins(0, 0, 0, 0);
    list_layout_->setSpacing(8);

    empty_label_ = new QLabel("All applications operating normally.\nNo critical resource alerts.", container);
    empty_label_->setObjectName("mutedLabel");
    empty_label_->setAlignment(Qt::AlignCenter);
    empty_label_->setStyleSheet("color: #9AA3AF; padding: 20px; font-style: italic;");
    list_layout_->addWidget(empty_label_);

    list_layout_->addStretch();
    scroll->setWidget(container);
    main_layout->addWidget(scroll);
}

void RecommendationsPanel::update_recommendations(const std::vector<pulse::core::Recommendation>& recs) {
    current_recs_ = recs;
    rebuild_ui();
}

void RecommendationsPanel::rebuild_ui() {
    // Clear existing item cards
    QLayoutItem* child;
    while ((child = list_layout_->takeAt(0)) != nullptr) {
        if (child->widget() && child->widget() != empty_label_) {
            delete child->widget();
        }
        delete child;
    }

    if (current_recs_.empty()) {
        empty_label_->show();
        list_layout_->addWidget(empty_label_);
        list_layout_->addStretch();
        return;
    }

    empty_label_->hide();

    for (const auto& rec : current_recs_) {
        if (rec.dismissed) continue;

        auto* item_card = new QFrame();
        item_card->setObjectName("cardFrame");
        item_card->setStyleSheet(
            "QFrame#cardFrame { "
            "  background-color: #161B1E; "
            "  border: 1px solid #2A3036; "
            "  border-radius: 6px; "
            "  padding: 10px; "
            "}");

        auto* card_layout = new QVBoxLayout(item_card);
        card_layout->setContentsMargins(10, 10, 10, 10);
        card_layout->setSpacing(6);

        // Header: Badge + Title + Dismiss Button
        auto* top_row = new QHBoxLayout();
        
        QString badge_col = "#7C3AED"; // Info purple
        QString badge_text = "INFO";
        if (rec.severity == pulse::core::RuleSeverity::Warning) {
            badge_col = "#F59E0B"; // Amber
            badge_text = "WARNING";
        } else if (rec.severity == pulse::core::RuleSeverity::Critical) {
            badge_col = "#EF4444"; // Red
            badge_text = "CRITICAL";
        }

        auto* badge = new QLabel(badge_text, item_card);
        badge->setStyleSheet(QString(
            "background-color: %1; color: #FFFFFF; font-size: 10px; font-weight: bold; "
            "padding: 2px 6px; border-radius: 4px;").arg(badge_col));
        top_row->addWidget(badge);

        auto* item_title = new QLabel(QString::fromStdString(rec.title), item_card);
        item_title->setFont(QFont("Segoe UI", 9, QFont::Bold));
        item_title->setStyleSheet("color: #FFFFFF;");
        top_row->addWidget(item_title, 1);

        auto* dismiss_btn = new QPushButton("✕", item_card);
        dismiss_btn->setFixedSize(22, 22);
        dismiss_btn->setToolTip("Dismiss recommendation");
        dismiss_btn->setStyleSheet(
            "QPushButton { background: transparent; border: none; color: #9AA3AF; font-size: 12px; }"
            "QPushButton:hover { color: #EF4444; }");

        QString rec_id = QString::fromStdString(rec.id);
        connect(dismiss_btn, &QPushButton::clicked, this, [this, rec_id]() {
            emit dismiss_requested(rec_id);
        });
        top_row->addWidget(dismiss_btn);

        card_layout->addLayout(top_row);

        // Message
        auto* msg_label = new QLabel(QString::fromStdString(rec.message), item_card);
        msg_label->setWordWrap(true);
        msg_label->setStyleSheet("color: #E5E7EB; font-size: 12px;");
        card_layout->addWidget(msg_label);

        // Suggestion
        if (!rec.suggestion.empty()) {
            auto* sug_label = new QLabel("Tip: " + QString::fromStdString(rec.suggestion), item_card);
            sug_label->setWordWrap(true);
            sug_label->setStyleSheet("color: #9AA3AF; font-size: 11px; font-style: italic;");
            card_layout->addWidget(sug_label);
        }

        list_layout_->addWidget(item_card);
    }

    list_layout_->addStretch();
}

} // namespace pulse::ui
