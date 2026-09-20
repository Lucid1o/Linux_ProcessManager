#pragma once

#include "pulse/core/process_info.hpp"
#include <QAbstractItemModel>
#include <QVector>
#include <memory>
#include <unordered_map>

namespace pulse::ui {

enum class ItemType {
    Application,
    Process
};

class TreeItem {
public:
    explicit TreeItem(TreeItem* parent = nullptr);
    ~TreeItem();

    void append_child(TreeItem* child);
    void remove_child(int row);
    TreeItem* child(int row);
    int child_count() const;
    int column_count() const;
    int row() const;
    TreeItem* parent_item();

    ItemType type{ItemType::Application};
    QString key_id; // app_id for apps, "p_" + pid for processes
    pulse::core::ApplicationGroup app_info;
    pulse::core::ProcessInfo proc_info;

private:
    TreeItem* parent_{nullptr};
    QVector<TreeItem*> children_;
};

class ProcessTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Column {
        ColName = 0,
        ColCpu = 1,
        ColMemory = 2,
        ColProcCountOrThreads = 3,
        ColPid = 4,
        ColPpid = 5,
        ColPriorityNice = 6,
        ColState = 7,
        ColUser = 8,
        ColCmdline = 9,
        ColumnCount = 10
    };

    explicit ProcessTreeModel(QObject* parent = nullptr);
    ~ProcessTreeModel() override;

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Incremental update (does NOT collapse expanded rows!)
    void update_data(const std::vector<pulse::core::ApplicationGroup>& apps);

    // Helpers to retrieve selected items
    bool is_application_index(const QModelIndex& index) const;
    std::optional<pulse::core::ApplicationGroup> get_application(const QModelIndex& index) const;
    std::optional<pulse::core::ProcessInfo> get_process(const QModelIndex& index) const;

private:
    TreeItem* root_item_{nullptr};

    void sync_process_children(
        TreeItem* parent_item,
        const QModelIndex& parent_index,
        const std::vector<std::shared_ptr<pulse::core::ProcessNode>>& child_nodes);

    static QString format_bytes(uint64_t bytes);
};

} // namespace pulse::ui
