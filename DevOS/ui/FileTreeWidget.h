#pragma once

#include <QTreeWidget>
#include <QJsonObject>

class FileTreeWidget : public QTreeWidget {
    Q_OBJECT
public:
    explicit FileTreeWidget(QWidget *parent = nullptr);

public slots:
    void loadPlan(const QJsonObject &plan);
};
