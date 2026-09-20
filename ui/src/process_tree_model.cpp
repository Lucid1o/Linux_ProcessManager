#include "pulse/ui/process_tree_model.hpp"
#include <QColor>
#include <QFont>
#include <QIcon>
#include <algorithm>

namespace pulse::ui {

TreeItem::TreeItem(TreeItem* parent)
    : parent_(parent) {}

TreeItem::~TreeItem() {
    qDeleteAll(children_);
}

void TreeItem::append_child(TreeItem* child) {
    child->parent_ = this;
    children_.push_back(child);
}

void TreeItem::remove_child(int row) {
    if (row >= 0 && row < children_.size()) {
        delete children_.takeAt(row);
    }
}

TreeItem* TreeItem::child(int row) {
    if (row >= 0 && row < children_.size()) {
        return children_[row];
    }
    return nullptr;
}

int TreeItem::child_count() const {
    return children_.size();
}

int TreeItem::column_count() const {
    return ProcessTreeModel::ColumnCount;
}

int TreeItem::row() const {
    if (parent_) {
        return parent_->children_.indexOf(const_cast<TreeItem*>(this));
    }
    return 0;
}

TreeItem* TreeItem::parent_item() {
    return parent_;
}

// ---------------- ProcessTreeModel ----------------

ProcessTreeModel::ProcessTreeModel(QObject* parent)
    : QAbstractItemModel(parent) {
    root_item_ = new TreeItem(nullptr);
}

ProcessTreeModel::~ProcessTreeModel() {
    delete root_item_;
}

int ProcessTreeModel::columnCount(const QModelIndex& /*parent*/) const {
    return ColumnCount;
}

QModelIndex ProcessTreeModel::index(int row, int column, const QModelIndex& parent) const {
    if (!hasIndex(row, column, parent)) return QModelIndex();

    TreeItem* parent_item = nullptr;
    if (!parent.isValid()) {
        parent_item = root_item_;
    } else {
        parent_item = static_cast<TreeItem*>(parent.internalPointer());
    }

    TreeItem* child_item = parent_item->child(row);
    if (child_item) {
        return createIndex(row, column, child_item);
    }
    return QModelIndex();
}

QModelIndex ProcessTreeModel::parent(const QModelIndex& child) const {
    if (!child.isValid()) return QModelIndex();

    auto* child_item = static_cast<TreeItem*>(child.internalPointer());
    TreeItem* parent_item = child_item->parent_item();

    if (parent_item == root_item_ || !parent_item) {
        return QModelIndex();
    }

    return createIndex(parent_item->row(), 0, parent_item);
}

int ProcessTreeModel::rowCount(const QModelIndex& parent) const {
    TreeItem* parent_item = nullptr;
    if (parent.column() > 0) return 0;

    if (!parent.isValid()) {
        parent_item = root_item_;
    } else {
        parent_item = static_cast<TreeItem*>(parent.internalPointer());
    }

    return parent_item ? parent_item->child_count() : 0;
}

QString ProcessTreeModel::format_bytes(uint64_t bytes) {
    const double kb = 1024.0;
    const double mb = kb * 1024.0;
    const double gb = mb * 1024.0;

    double d = static_cast<double>(bytes);
    if (d >= gb) {
        return QString::asprintf("%.1f GB", d / gb);
    } else if (d >= mb) {
        return QString::asprintf("%.1f MB", d / mb);
    } else if (d >= kb) {
        return QString::asprintf("%.0f KB", d / kb);
    }
    return QString::asprintf("%llu B", static_cast<unsigned long long>(bytes));
}

QVariant ProcessTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return QVariant();

    auto* item = static_cast<TreeItem*>(index.internalPointer());
    int col = index.column();

    if (role == Qt::DisplayRole) {
        if (item->type == ItemType::Application) {
            const auto& app = item->app_info;
            switch (col) {
                case ColName:
                    return QString::fromStdString(app.display_name);
                case ColCpu:
                    return QString::asprintf("%.1f%%", app.total_cpu_percent);
                case ColMemory:
                    return format_bytes(app.total_memory_rss_bytes);
                case ColProcCountOrThreads:
                    return QString("%1 procs (%2 thr)").arg(app.process_count).arg(app.total_threads);
                case ColPid:
                    return QString::number(app.root_pid);
                case ColPpid:
                    return "-";
                case ColPriorityNice:
                    return "-";
                case ColState:
                    return "Active";
                case ColUser:
                    return "-";
                case ColCmdline:
                    return QString::fromStdString(app.app_id);
                default:
                    return QVariant();
            }
        } else {
            const auto& p = item->proc_info;
            switch (col) {
                case ColName:
                    return QString::fromStdString(p.name);
                case ColCpu:
                    return QString::asprintf("%.1f%%", p.cpu_percent);
                case ColMemory:
                    return format_bytes(p.memory_rss_bytes);
                case ColProcCountOrThreads:
                    return QString("%1 threads").arg(p.num_threads);
                case ColPid:
                    return QString::number(p.pid);
                case ColPpid:
                    return QString::number(p.ppid);
                case ColPriorityNice:
                    return QString("%1 / %2").arg(p.priority).arg(p.nice);
                case ColState:
                    return QString(process_state_to_string(p.state));
                case ColUser:
                    return QString::fromStdString(p.user.empty() ? std::to_string(p.uid) : p.user);
                case ColCmdline:
                    return QString::fromStdString(p.cmdline);
                default:
                    return QVariant();
            }
        }
    } else if (role == Qt::ForegroundRole) {
        if (item->type == ItemType::Process) {
            if (item->proc_info.state == pulse::core::ProcessState::Zombie) {
                return QColor(0xEF, 0x44, 0x44); // Red for zombie
            }
            if (item->proc_info.cpu_percent > 50.0) {
                return QColor(0xC6, 0xF1, 0x35); // Lime for high cpu
            }
        } else {
            if (col == ColCpu && item->app_info.total_cpu_percent > 50.0) {
                return QColor(0xC6, 0xF1, 0x35);
            }
        }
    } else if (role == Qt::FontRole) {
        if (item->type == ItemType::Application && col == ColName) {
            QFont f;
            f.setBold(true);
            return f;
        }
    }

    return QVariant();
}

QVariant ProcessTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case ColName: return "Application / Process";
            case ColCpu: return "CPU %";
            case ColMemory: return "Memory (RSS)";
            case ColProcCountOrThreads: return "Processes / Threads";
            case ColPid: return "PID";
            case ColPpid: return "PPID";
            case ColPriorityNice: return "Pri / Nice";
            case ColState: return "State";
            case ColUser: return "User";
            case ColCmdline: return "Command Line";
            default: break;
        }
    }
    return QVariant();
}

void ProcessTreeModel::update_data(const std::vector<pulse::core::ApplicationGroup>& apps) {
    // Incremental update algorithm to prevent tree collapse:
    // 1. Match top-level applications by app_id
    std::unordered_map<std::string, int> existing_app_rows;
    for (int r = 0; r < root_item_->child_count(); ++r) {
        existing_app_rows[root_item_->child(r)->key_id.toStdString()] = r;
    }

    std::unordered_set<std::string> new_app_ids;

    for (size_t i = 0; i < apps.size(); ++i) {
        const auto& app = apps[i];
        new_app_ids.insert(app.app_id);

        auto it = existing_app_rows.find(app.app_id);
        TreeItem* app_item = nullptr;
        QModelIndex app_index;

        if (it != existing_app_rows.end()) {
            // Existing application: update values in-place
            int row = it->second;
            app_item = root_item_->child(row);
            app_item->app_info = app;
            app_index = createIndex(row, 0, app_item);
            emit dataChanged(createIndex(row, 0, app_item), createIndex(row, ColumnCount - 1, app_item));
        } else {
            // New application: insert row
            int insert_row = root_item_->child_count();
            beginInsertRows(QModelIndex(), insert_row, insert_row);
            app_item = new TreeItem(root_item_);
            app_item->type = ItemType::Application;
            app_item->key_id = QString::fromStdString(app.app_id);
            app_item->app_info = app;
            root_item_->append_child(app_item);
            endInsertRows();
            app_index = createIndex(insert_row, 0, app_item);
        }

        // Sync process children recursively under this application
        sync_process_children(app_item, app_index, app.root_process_trees);
    }

    // Remove obsolete applications
    for (int r = root_item_->child_count() - 1; r >= 0; --r) {
        TreeItem* child = root_item_->child(r);
        if (new_app_ids.find(child->key_id.toStdString()) == new_app_ids.end()) {
            beginRemoveRows(QModelIndex(), r, r);
            root_item_->remove_child(r);
            endRemoveRows();
        }
    }
}

void ProcessTreeModel::sync_process_children(
    TreeItem* parent_item,
    const QModelIndex& parent_index,
    const std::vector<std::shared_ptr<pulse::core::ProcessNode>>& child_nodes) {

    std::unordered_map<std::string, int> existing_rows;
    for (int r = 0; r < parent_item->child_count(); ++r) {
        existing_rows[parent_item->child(r)->key_id.toStdString()] = r;
    }

    std::unordered_set<std::string> active_keys;

    for (const auto& node : child_nodes) {
        if (!node) continue;
        std::string key = "p_" + std::to_string(node->info.pid);
        active_keys.insert(key);

        TreeItem* proc_item = nullptr;
        QModelIndex child_index;

        auto it = existing_rows.find(key);
        if (it != existing_rows.end()) {
            int row = it->second;
            proc_item = parent_item->child(row);
            proc_item->proc_info = node->info;
            child_index = createIndex(row, 0, proc_item);
            emit dataChanged(createIndex(row, 0, proc_item), createIndex(row, ColumnCount - 1, proc_item));
        } else {
            int insert_row = parent_item->child_count();
            beginInsertRows(parent_index, insert_row, insert_row);
            proc_item = new TreeItem(parent_item);
            proc_item->type = ItemType::Process;
            proc_item->key_id = QString::fromStdString(key);
            proc_item->proc_info = node->info;
            parent_item->append_child(proc_item);
            endInsertRows();
            child_index = createIndex(insert_row, 0, proc_item);
        }

        // Recurse for deeper children
        sync_process_children(proc_item, child_index, node->children);
    }

    // Remove exited processes
    for (int r = parent_item->child_count() - 1; r >= 0; --r) {
        TreeItem* child = parent_item->child(r);
        if (active_keys.find(child->key_id.toStdString()) == active_keys.end()) {
            beginRemoveRows(parent_index, r, r);
            parent_item->remove_child(r);
            endRemoveRows();
        }
    }
}

bool ProcessTreeModel::is_application_index(const QModelIndex& index) const {
    if (!index.isValid()) return false;
    auto* item = static_cast<TreeItem*>(index.internalPointer());
    return item && item->type == ItemType::Application;
}

std::optional<pulse::core::ApplicationGroup> ProcessTreeModel::get_application(const QModelIndex& index) const {
    if (!index.isValid()) return std::nullopt;
    auto* item = static_cast<TreeItem*>(index.internalPointer());
    if (item && item->type == ItemType::Application) {
        return item->app_info;
    }
    return std::nullopt;
}

std::optional<pulse::core::ProcessInfo> ProcessTreeModel::get_process(const QModelIndex& index) const {
    if (!index.isValid()) return std::nullopt;
    auto* item = static_cast<TreeItem*>(index.internalPointer());
    if (item && item->type == ItemType::Process) {
        return item->proc_info;
    }
    if (item && item->type == ItemType::Application) {
        // App root process info
        pulse::core::ProcessInfo p;
        p.pid = item->app_info.root_pid;
        p.name = item->app_info.display_name;
        return p;
    }
    return std::nullopt;
}

} // namespace pulse::ui
