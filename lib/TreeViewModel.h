#pragma once

#include <QAbstractProxyModel>
#include "TreeItemViewModel.h"

/**
 * @brief Proxy model that flattens any source TreeModel to make it suitable to display in a qml ListView (see TreeView.qml).
 *
 * It adds the following roles that can be used by the tree item delegate (see TreeItemView.qml):
 *  - indentation
 *  - hasChildren
 *  - isExpanded
 *  - hidden
 *
 * Use the setSourceModel method to set the source TreeModel (e.g. QFileSystemModel)
 */
class TreeViewModel: public QAbstractProxyModel {
    enum TreeRoles {
        Indentation = Qt::UserRole + 1,
        HasChildren,
        IsExpanded,
        Hidden
    };
public:
    TreeViewModel(QObject* parent= nullptr) : QAbstractProxyModel(parent)
    {
    }

//    /**
//     * Sets the root item to the item at the given source index.
//     *
//     * This is equivalent to QTreeView::setRootIndex
//     *
//     * @param sourceRootIndex root index from sourceModel
//     */
//    void setRootIndex(const QModelIndex &sourceRootIndex)
//    {
//        // TODO
//        beginResetModel();
//        rootItem_ = findItemByIndex(sourceRootIndex);
//
//        endResetModel();
//    }

    // QAbstactProxyModel implementation
    void setSourceModel(QAbstractItemModel *sourceModel) override
    {
        QAbstractProxyModel::setSourceModel(sourceModel);
        doResetModel(sourceModel);

        if (sourceModel != nullptr) {
            connect(sourceModel, &QAbstractItemModel::dataChanged, this, &TreeViewModel::onSourceDataChanged);
            connect(sourceModel, &QAbstractItemModel::rowsInserted, this, &TreeViewModel::onRowsInserted);
            connect(sourceModel, &QAbstractItemModel::rowsRemoved, this, &TreeViewModel::onRowsRemoved);
        }
    }

    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override
    {
        if (!proxyIndex.isValid() || !flattenedTree_.count() < proxyIndex.row())
            return QModelIndex();
        return flattenedTree_[proxyIndex.row()]->sourceIndex();
    }

    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override
    {
        TreeItemViewModel* n = findItemByIndex(sourceIndex);
        if (n == nullptr)
            return QModelIndex();
        return createIndex(n->row(), 0);
    }

    int columnCount(const QModelIndex &parent) const override
    {
        return sourceModel()->columnCount(mapToSource(parent));
    }

    int rowCount(const QModelIndex &parent) const override
    {
        if (!parent.isValid())
            return flattenedTree_.count();
        return 0;
    }

    QModelIndex index(int row, int column, const QModelIndex &parent) const override
    {
        if (parent.isValid())
            return QModelIndex();
        else
            return createIndex(row, column);
    }

    QModelIndex parent(const QModelIndex &child) const override
    {
        return QModelIndex();
    }

    QVariant data(const QModelIndex &proxyIndex, int role) const override
    {
        switch (role) {
            case Indentation:
                return flattenedTree_[proxyIndex.row()]->indent();
            case HasChildren:
                return flattenedTree_[proxyIndex.row()]->hasChildren();
            case IsExpanded:
                return flattenedTree_[proxyIndex.row()]->isExpanded();
            case Hidden:
                return flattenedTree_[proxyIndex.row()]->isHidden();
            default:
                return flattenedTree_[proxyIndex.row()]->sourceIndex().data(role);
        }
    }

    bool setData(const QModelIndex &proxyIndex, const QVariant &value, int role) override
    {
        switch (role) {
            case IsExpanded:
                toggleIsExpanded(proxyIndex.row(), value.toBool());
                break;
            default:
                QAbstractProxyModel::setData(proxyIndex, value, role);
        }
    }

    QHash<int, QByteArray> roleNames() const override
    {
        auto names = QAbstractItemModel::roleNames();
        names[Indentation] = "indentation";
        names[HasChildren] = "hasChildren";
        names[IsExpanded] = "isExpanded";
        names[Hidden] = "hidden";
        return names;
    }

private slots:
    void onSourceDataChanged(QModelIndex topLeft, QModelIndex bottomRight)
    {
        emit dataChanged(mapFromSource(topLeft), mapFromSource(bottomRight));
    }

    void onRowsInserted(const QModelIndex& parent, int first, int last)
    {
        TreeItemViewModel* parentNode = findItemByIndex(parent);

        if (parentNode == nullptr)
            return;

        int firstRow = 0;
        int lastRow = 0;

        for (int row = first; row < last + 1; ++row) {
            QModelIndex childIndex = parent.child(row, 0);
            TreeItemViewModel* n = parentNode->addChild(childIndex);
            if (row == first)
                firstRow = n->row();
            else if (row == last)
                lastRow = n->row();
        }
        beginInsertRows(QModelIndex(), firstRow, lastRow);

        endInsertRows();
    }

    void onRowsRemoved(const QModelIndex& parent, int first, int last)
    {
        // TODO
    }

private:
    int flatten(QAbstractItemModel *model, QModelIndex parent = QModelIndex(), TreeItemViewModel* parentNode= nullptr)
    {
        if (parentNode == nullptr) {
            qDeleteAll(flattenedTree_);
            flattenedTree_.clear();
        }

        if (model == nullptr)
            return 0;

        auto rows = model->rowCount(parent);

        for(int rowIndex = 0; rowIndex < rows; ++rowIndex) {
            QModelIndex index = model->index(rowIndex, 0, parent);

            TreeItemViewModel* node = nullptr;
            if (parentNode)
                node = parentNode->addChild(index);
            else
                node = new TreeItemViewModel(parentNode, index, flattenedTree_, this);

            if (node->hasChildren())
                flatten(model, index, node);
        }
    }

    void toggleIsExpanded(int row, bool isExpanded)
    {
        flattenedTree_[row]->setExpanded(isExpanded);
    }

    void doResetModel(QAbstractItemModel *sourceModel)
    {
        beginResetModel();
        flatten(sourceModel);
        endResetModel();
    }

    TreeItemViewModel* findItemByIndex(const QModelIndex &sourceIndex) const
    {
        for (TreeItemViewModel* n: flattenedTree_) {
            if (n->sourceIndex() == sourceIndex)
                return n;
        }
        return nullptr;
    }

    QList<TreeItemViewModel*> flattenedTree_;
//    TreeItemViewModel* rootItem_ = nullptr;
};

