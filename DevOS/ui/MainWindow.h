#pragma once

#include <QMainWindow>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>

class ApiClient;
class WebSocketClient;
class IDELauncher;
class FileTreeWidget;
class LogViewerWidget;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QComboBox;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onGenerateClicked();
    void onExecuteClicked();
    void onLocalCommandFinished(int exitCode, QProcess::ExitStatus status);
    void onPlanGenerated(const QJsonObject &plan);
    void onExecutionStarted(const QString &execId);
    void onExecutionStatus(const QJsonObject &status);
    void onNetworkError(const QString &message);
    void onLogReceived(const QString &line);
    void onSocketStatusUpdated(const QString &status);
    void onIdePreferenceChanged(int index);
    void pollStatus();

private:
    void buildUi();
    void setStatusText(const QString &text);
    void setCommandText(const QString &text);
    void renderCommands(const QJsonArray &commands);

protected:
    void keyPressEvent(QKeyEvent *event) override;

    ApiClient *m_apiClient;
    WebSocketClient *m_wsClient;
    IDELauncher *m_ideLauncher;
    FileTreeWidget *m_tree;
    LogViewerWidget *m_logs;
    QPlainTextEdit *m_prompt;
    QPushButton *m_generateBtn;
    QPushButton *m_executeBtn;
    QComboBox *m_ideSelect;
    QLabel *m_statusLabel;
    QLabel *m_commandLabel;
    QLabel *m_titleLabel;
    QLabel *m_taglineLabel;
    QTimer *m_pollTimer;

    QJsonObject m_plan;
    QString m_executionId;
    QString m_workspacePath;
    bool m_ideLaunched{false};
    QProcess *m_cmdProcess{nullptr};
    QStringList m_pendingCommands;
    int m_currentCommandIndex{0};
};
