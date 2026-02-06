#include "FileTreeWidget.h"

#include <QJsonArray>
#include <QSet>

FileTreeWidget::FileTreeWidget(QWidget *parent) : QTreeWidget(parent) {
    setHeaderHidden(true);
}

void FileTreeWidget::loadPlan(const QJsonObject &plan) {
    clear();
    QSet<QString> folderSet;
    const auto folders = plan.value("folders").toArray();
    for (const auto &f : folders) {
        folderSet.insert(f.toString());
    }
    const auto files = plan.value("files").toArray();
    for (const auto &f : files) {
        const auto obj = f.toObject();
        folderSet.insert(obj.value("path").toString());
    }

    QMap<QString, QTreeWidgetItem *> nodes;
    auto ensureNode = [&](const QString &path) -> QTreeWidgetItem * {
        if (nodes.contains(path)) return nodes[path];
        const QStringList parts = path.split('/', Qt::SkipEmptyParts);
        QString current;
        QTreeWidgetItem *parent = nullptr;
        for (const auto &part : parts) {
            current = current.isEmpty() ? part : current + "/" + part;
            if (!nodes.contains(current)) {
                auto *item = new QTreeWidgetItem(QStringList(part));
                if (parent) parent->addChild(item); else addTopLevelItem(item);
                nodes[current] = item;
                parent = item;
            } else {
                parent = nodes[current];
            }
        }
        return nodes[path];
    };

    for (const auto &f : folders) {
        ensureNode(f.toString());
    }
    for (const auto &f : files) {
        ensureNode(f.toObject().value("path").toString());
    }
    expandAll();
}
