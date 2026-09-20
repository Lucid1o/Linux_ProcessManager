#pragma once

#include "pulse/collector/system_collector.hpp"
#include "pulse/core/app_grouper.hpp"
#include "pulse/core/process_manager.hpp"
#include "pulse/core/rule_engine.hpp"
#include "pulse/ui/dashboard_widget.hpp"
#include "pulse/ui/process_tree_model.hpp"
#include "pulse/ui/recommendations_panel.hpp"

#include <QMainWindow>
#include <QTreeView>
#include <QLineEdit>
#include <QToolButton>
#include <QSplitter>
#include <QTimer>
#include <QSortFilterProxyModel>
#include <memory>

namespace pulse::ui {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(
        std::shared_ptr<pulse::collector::ISystemCollector> collector,
        std::shared_ptr<pulse::core::IProcessController> controller,
        QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_refresh_tick();
    void on_mode_toggled(bool advanced);
    void on_search_text_changed(const QString& text);
    void on_tree_context_menu(const QPoint& pos);
    void on_action_close_normal();
    void on_action_force_close();
    void on_action_close_entire_app();
    void on_dismiss_recommendation(const QString& rec_id);

private:
    std::shared_ptr<pulse::collector::ISystemCollector> collector_;
    std::shared_ptr<pulse::core::ProcessManager> process_manager_;
    pulse::core::AppGrouper app_grouper_;
    pulse::core::RuleEngine rule_engine_;

    // UI Widgets
    DashboardWidget* dashboard_{nullptr};
    QTreeView* tree_view_{nullptr};
    ProcessTreeModel* tree_model_{nullptr};
    QSortFilterProxyModel* proxy_model_{nullptr};
    RecommendationsPanel* recs_panel_{nullptr};
    QLineEdit* search_edit_{nullptr};
    QToolButton* mode_btn_{nullptr};
    QTimer* refresh_timer_{nullptr};

    bool is_advanced_mode_{false};

    void setup_ui();
    void setup_toolbar();
    void apply_column_visibility();
    QModelIndex get_selected_source_index() const;
};

} // namespace pulse::ui
