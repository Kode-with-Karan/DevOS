#pragma once

#include <QPlainTextEdit>

class LogViewerWidget : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit LogViewerWidget(QWidget *parent = nullptr);

public slots:
    void appendLog(const QString &line);
};
