#include "pulse/ui/main_window.hpp"
#include "pulse/ui/action_dialogs.hpp"

#include <QToolBar>
#include <QLabel>
#include <QMenu>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QStatusBar>
#include <QApplication>
#include <QFile>

namespace pulse::ui {

MainWindow::MainWindow(
    std::shared_ptr<pulse::collector::ISystemCollector> collector,
    std::shared_ptr<pulse::core::IProcessController> controller,
    QWidget* parent)
    : QMainWindow(parent)
    , collector_(std::move(collector))
    , process_manager_(std::make_shared<pulse::core::ProcessManager>(std::move(controller))) {

    setWindowTitle("Pulse — Application-Centric System Manager");
    resize(1200, 780);
    setMinimumSize(960, 600);

    setup_ui();
    setup_toolbar();
    apply_column_visibility();

    // Start 1-second refresh timer
    refresh_timer_ = new QTimer(this);
    connect(refresh_timer_, &QTimer::timeout, this, &MainWindow::on_refresh_tick);
    refresh_timer_->start(1000);

    // Initial tick to populate immediate data
    on_refresh_tick();
}

MainWindow::~MainWindow() = default;

void MainWindow::setup_ui() {
    auto* central_widget = new QWidget(this);
    setCentralWidget(central_widget);

    auto* main_layout = new QVBoxLayout(central_widget);
    main_layout->setContentsMargins(16, 16, 16, 16);
    main_layout->setSpacing(14);

    // 1. Dashboard Widget (4 Cards with Sparklines)
    dashboard_ = new DashboardWidget(this);
    main_layout->addWidget(dashboard_);

    // 2. Center Splitter: Main Application Tree & Recommendations Panel
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(8);
    splitter->setStyleSheet(
        "QSplitter::handle { background-color: #101416; border-left: 1px solid #2A3036; border-right: 1px solid #2A3036; }");

    // Process Tree View
    tree_view_ = new QTreeView(splitter);
    tree_model_ = new ProcessTreeModel(this);
    proxy_model_ = new QSortFilterProxyModel(this);
    proxy_model_->setSourceModel(tree_model_);
    proxy_model_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy_model_->setRecursiveFilteringEnabled(true);
    proxy_model_->setFilterKeyColumn(-1); // Filter across all columns

    tree_view_->setModel(proxy_model_);
    tree_view_->setAlternatingRowColors(true);
    tree_view_->setUniformRowHeights(true);
    tree_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_view_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_view_->header()->setStretchLastSection(true);
    tree_view_->header()->setSectionsMovable(true);

    connect(tree_view_, &QTreeView::customContextMenuRequested, this, &MainWindow::on_tree_context_menu);
    splitter->addWidget(tree_view_);

    // Recommendations Panel
    recs_panel_ = new RecommendationsPanel(splitter);
    connect(recs_panel_, &RecommendationsPanel::dismiss_requested, this, &MainWindow::on_dismiss_recommendation);
    splitter->addWidget(recs_panel_);

    // Set splitter sizes (70% tree, 30% panel)
    splitter->setSizes({820, 340});
    main_layout->addWidget(splitter, 1);

    // Status Bar
    statusBar()->showMessage("Pulse ready. Monitoring active Linux processes.");
}

void MainWindow::setup_toolbar() {
    auto* toolbar = addToolBar("Main Controls");
    toolbar->setMovable(false);
    toolbar->setFloatable(false);

    // App Logo / Title
    auto* app_label = new QLabel("PULSE", this);
    app_label->setFont(QFont("Segoe UI", 12, QFont::Black));
    app_label->setStyleSheet("color: #C6F135; padding: 0 8px; font-weight: 900;");
    toolbar->addWidget(app_label);

    toolbar->addSeparator();

    // Mode Toggle Button (Beginner / Advanced)
    mode_btn_ = new QToolButton(this);
    mode_btn_->setText("⚡ Mode: Beginner");
    mode_btn_->setCheckable(true);
    mode_btn_->setChecked(false);
    mode_btn_->setToolTip("Toggle between simplified Beginner view and detailed Advanced view");
    connect(mode_btn_, &QToolButton::toggled, this, &MainWindow::on_mode_toggled);
    toolbar->addWidget(mode_btn_);

    toolbar->addSeparator();

    // Close Normally Action
    auto* close_btn = new QToolButton(this);
    close_btn->setText("Close Normally");
    close_btn->setToolTip("Send SIGTERM to the selected application or process");
    connect(close_btn, &QToolButton::clicked, this, &MainWindow::on_action_close_normal);
    toolbar->addWidget(close_btn);

    // Force Close Action
    auto* force_btn = new QToolButton(this);
    force_btn->setText("Force Close");
    force_btn->setToolTip("Send SIGKILL after confirmation");
    force_btn->setObjectName("dangerButton");
    connect(force_btn, &QToolButton::clicked, this, &MainWindow::on_action_force_close);
    toolbar->addWidget(force_btn);

    // Close Entire Application Action
    auto* entire_btn = new QToolButton(this);
    entire_btn->setText("Close Entire App");
    entire_btn->setToolTip("Gracefully terminate whole application tree (children first, 5s timeout)");
    connect(entire_btn, &QToolButton::clicked, this, &MainWindow::on_action_close_entire_app);
    toolbar->addWidget(entire_btn);

    toolbar->addSeparator();

    // Search / Filter Box
    search_edit_ = new QLineEdit(this);
    search_edit_->setPlaceholderText("🔍 Filter applications or PIDs...");
    search_edit_->setClearButtonEnabled(true);
    search_edit_->setMaximumWidth(280);
    connect(search_edit_, &QLineEdit::textChanged, this, &MainWindow::on_search_text_changed);
    toolbar->addWidget(search_edit_);

    // Spacer to push interval badge to the right
    auto* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);

    auto* interval_badge = new QLabel("Refresh: 1.0s", this);
    interval_badge->setStyleSheet("color: #9AA3AF; font-size: 11px; padding: 0 8px;");
    toolbar->addWidget(interval_badge);
}

void MainWindow::apply_column_visibility() {
    // Beginner columns: ColName, ColCpu, ColMemory, ColProcCountOrThreads
    // Advanced columns: ColPid, ColPpid, ColPriorityNice, ColState, ColUser, ColCmdline
    tree_view_->setColumnHidden(ProcessTreeModel::ColPid, !is_advanced_mode_);
    tree_view_->setColumnHidden(ProcessTreeModel::ColPpid, !is_advanced_mode_);
    tree_view_->setColumnHidden(ProcessTreeModel::ColPriorityNice, !is_advanced_mode_);
    tree_view_->setColumnHidden(ProcessTreeModel::ColState, !is_advanced_mode_);
    tree_view_->setColumnHidden(ProcessTreeModel::ColUser, !is_advanced_mode_);
    tree_view_->setColumnHidden(ProcessTreeModel::ColCmdline, !is_advanced_mode_);

    // Adjust column widths
    tree_view_->setColumnWidth(ProcessTreeModel::ColName, 260);
    tree_view_->setColumnWidth(ProcessTreeModel::ColCpu, 90);
    tree_view_->setColumnWidth(ProcessTreeModel::ColMemory, 110);
    tree_view_->setColumnWidth(ProcessTreeModel::ColProcCountOrThreads, 150);
    if (is_advanced_mode_) {
        tree_view_->setColumnWidth(ProcessTreeModel::ColPid, 70);
        tree_view_->setColumnWidth(ProcessTreeModel::ColPpid, 70);
        tree_view_->setColumnWidth(ProcessTreeModel::ColPriorityNice, 85);
        tree_view_->setColumnWidth(ProcessTreeModel::ColState, 100);
        tree_view_->setColumnWidth(ProcessTreeModel::ColUser, 80);
    }
}

void MainWindow::on_mode_toggled(bool advanced) {
    is_advanced_mode_ = advanced;
    mode_btn_->setText(advanced ? "⚡ Mode: Advanced" : "⚡ Mode: Beginner");
    apply_column_visibility();
    statusBar()->showMessage(advanced ? "Switched to Advanced Mode (Detailed process inspection)"
                                      : "Switched to Beginner Mode (Application overview)");
}

void MainWindow::on_search_text_changed(const QString& text) {
    proxy_model_->setFilterFixedString(text);
    if (!text.isEmpty()) {
        tree_view_->expandAll();
    }
}

void MainWindow::on_refresh_tick() {
    if (!collector_) return;

    // 1. Collect telemetry & processes
    collector_->update();
    const auto& metrics = collector_->get_system_metrics();
    const auto& raw_procs = collector_->get_processes();

    // 2. Update dashboard cards
    dashboard_->update_metrics(metrics);

    // 3. Group processes into applications
    auto app_groups = app_grouper_.group_processes(raw_procs);

    // 4. Update tree model incrementally (preserves expanded nodes!)
    tree_model_->update_data(app_groups);

    // 5. Evaluate recommendation rules
    auto recs = rule_engine_.evaluate(app_groups, raw_procs, metrics);
    recs_panel_->update_recommendations(recs);
}

QModelIndex MainWindow::get_selected_source_index() const {
    auto selected_indexes = tree_view_->selectionModel()->selectedRows();
    if (selected_indexes.isEmpty()) return QModelIndex();
    return proxy_model_->mapToSource(selected_indexes.first());
}

void MainWindow::on_tree_context_menu(const QPoint& pos) {
    QModelIndex proxy_idx = tree_view_->indexAt(pos);
    if (!proxy_idx.isValid()) return;

    QModelIndex src_idx = proxy_model_->mapToSource(proxy_idx);
    bool is_app = tree_model_->is_application_index(src_idx);

    QMenu menu(this);
    QAction* act_close = menu.addAction("Close Normally (SIGTERM)");
    QAction* act_force = menu.addAction("Force Close (SIGKILL)...");
    QAction* act_entire = nullptr;
    if (is_app) {
        menu.addSeparator();
        act_entire = menu.addAction("Close Entire Application Tree...");
    }

    QAction* chosen = menu.exec(tree_view_->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == act_close) {
        on_action_close_normal();
    } else if (chosen == act_force) {
        on_action_force_close();
    } else if (chosen == act_entire) {
        on_action_close_entire_app();
    }
}

void MainWindow::on_action_close_normal() {
    QModelIndex src_idx = get_selected_source_index();
    if (!src_idx.isValid()) {
        statusBar()->showMessage("Please select an application or process first.");
        return;
    }

    auto proc_opt = tree_model_->get_process(src_idx);
    if (!proc_opt.has_value()) return;

    auto report = process_manager_->close_normally(*proc_opt);
    ActionDialogs::show_termination_report(this, "Close Normally", report);
    on_refresh_tick();
}

void MainWindow::on_action_force_close() {
    QModelIndex src_idx = get_selected_source_index();
    if (!src_idx.isValid()) {
        statusBar()->showMessage("Please select an application or process first.");
        return;
    }

    auto proc_opt = tree_model_->get_process(src_idx);
    if (!proc_opt.has_value()) return;

    QString name = QString::fromStdString(proc_opt->name);
    if (!ActionDialogs::confirm_force_close(this, name, proc_opt->pid)) {
        return;
    }

    auto report = process_manager_->force_close(*proc_opt);
    ActionDialogs::show_termination_report(this, "Force Close", report);
    on_refresh_tick();
}

void MainWindow::on_action_close_entire_app() {
    QModelIndex src_idx = get_selected_source_index();
    if (!src_idx.isValid()) {
        statusBar()->showMessage("Please select an application first.");
        return;
    }

    auto app_opt = tree_model_->get_application(src_idx);
    if (!app_opt.has_value()) {
        // If child process selected, find parent app
        QModelIndex p_idx = src_idx.parent();
        while (p_idx.isValid()) {
            app_opt = tree_model_->get_application(p_idx);
            if (app_opt.has_value()) break;
            p_idx = p_idx.parent();
        }
    }

    if (!app_opt.has_value()) {
        statusBar()->showMessage("Selected item is not part of an application group.");
        return;
    }

    ActionDialogs::execute_graceful_app_close(this, *app_opt, process_manager_);
    on_refresh_tick();
}

void MainWindow::on_dismiss_recommendation(const QString& rec_id) {
    rule_engine_.dismiss_recommendation(rec_id.toStdString());
    on_refresh_tick();
}

} // namespace pulse::ui
