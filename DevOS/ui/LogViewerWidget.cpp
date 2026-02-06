#include "LogViewerWidget.h"

#include <QFontDatabase>
#include <QScrollBar>

LogViewerWidget::LogViewerWidget(QWidget *parent) : QPlainTextEdit(parent) {
    setReadOnly(true);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    setFont(mono);
}

void LogViewerWidget::appendLog(const QString &line) {
    appendPlainText(line);
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}
