#include "MainWindow.h"

#include "core/ApiClient.h"
#include "core/IDELauncher.h"
#include "core/SandboxValidator.h"
#include "core/WebSocketClient.h"
#include "ui/FileTreeWidget.h"
#include "ui/LogViewerWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QFileDialog>
#include <QProcess>
#include <QTimer>
#include <QJsonArray>
#include <QMessageBox>
#include <QWidget>
#include <QUrl>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QKeyEvent>

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent),
            m_apiClient(new ApiClient(this)),
            m_wsClient(new WebSocketClient(this)),
            m_ideLauncher(new IDELauncher(this)),
            m_tree(new FileTreeWidget(this)),
            m_logs(new LogViewerWidget(this)),
            m_prompt(new QPlainTextEdit(this)),
            m_generateBtn(new QPushButton("Generate Plan", this)),
            m_executeBtn(new QPushButton("Execute", this)),
            m_ideSelect(new QComboBox(this)),
            m_statusLabel(new QLabel("Idle", this)),
            m_commandLabel(new QLabel("", this)),
            m_pollTimer(new QTimer(this)) {
        const QString envWorkspace = qEnvironmentVariable("DEVOS_WORKSPACE");
        m_workspacePath = envWorkspace.isEmpty() ? QDir::current().absoluteFilePath("workspace") : envWorkspace;

    buildUi();

    connect(m_generateBtn, &QPushButton::clicked, this, &MainWindow::onGenerateClicked);
    connect(m_executeBtn, &QPushButton::clicked, this, &MainWindow::onExecuteClicked);
    connect(m_ideSelect, &QComboBox::currentIndexChanged, this, &MainWindow::onIdePreferenceChanged);

    connect(m_apiClient, &ApiClient::planGenerated, this, &MainWindow::onPlanGenerated);
    connect(m_apiClient, &ApiClient::executionStarted, this, &MainWindow::onExecutionStarted);
    connect(m_apiClient, &ApiClient::executionStatusUpdated, this, &MainWindow::onExecutionStatus);
    connect(m_apiClient, &ApiClient::networkError, this, &MainWindow::onNetworkError);

    connect(m_wsClient, &WebSocketClient::logReceived, this, &MainWindow::onLogReceived);
    connect(m_wsClient, &WebSocketClient::statusUpdated, this, &MainWindow::onSocketStatusUpdated);
    connect(m_wsClient, &WebSocketClient::errorOccurred, this, [this](const QString &err) {
        m_logs->appendLog(QStringLiteral("[ws] %1").arg(err));
    });

    m_pollTimer->setInterval(2000);
    connect(m_pollTimer, &QTimer::timeout, this, &MainWindow::pollStatus);
}

void MainWindow::buildUi() {
    setStyleSheet(
        "QMainWindow { background-color: rgb(12, 0, 18); }"
        "QLabel { color: rgb(200, 200, 200); }"
        "QPlainTextEdit { background: rgb(0, 0, 0); color: rgb(220, 220, 220); border: 1px solid rgb(40, 40, 40); border-radius: 6px; }"
        "QPushButton { background: rgb(60, 20, 120); color: white; border: none; padding: 10px 14px; border-radius: 8px; font-weight: 600; }"
        "QPushButton:hover { background: rgb(90, 40, 160); }"
        "QComboBox { background: rgb(20, 20, 30); color: rgb(220, 220, 220); border: 1px solid rgb(60, 60, 80); border-radius: 6px; padding: 6px; }"
        "QTreeWidget { background: rgb(18, 18, 26); border: 1px solid rgb(40, 40, 60); border-radius: 8px; color: rgb(220, 220, 220); }"
        "QPlainTextEdit#logs { background: rgb(18, 18, 26); border: 1px solid rgb(40, 40, 60); border-radius: 8px; }"
    );

    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(32, 24, 32, 24);
    rootLayout->setSpacing(16);

    // Header block
    m_titleLabel = new QLabel("DevOS", this);
    QFont titleFont("Sans Serif", 48, QFont::Bold);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet("color: white;");

    m_taglineLabel = new QLabel("From prompt to project, skip setup and start building smarter.", this);
    QFont taglineFont("Leelawadee UI", 12);
    m_taglineLabel->setFont(taglineFont);
    m_taglineLabel->setStyleSheet("color: rgb(124, 124, 124);");

    rootLayout->addWidget(m_titleLabel, 0, Qt::AlignHCenter);
    rootLayout->addWidget(m_taglineLabel, 0, Qt::AlignHCenter);

    auto *topCard = new QFrame(this);
    topCard->setFrameShape(QFrame::NoFrame);
    auto *cardLayout = new QVBoxLayout(topCard);
    cardLayout->setContentsMargins(24, 18, 24, 18);
    cardLayout->setSpacing(12);

    m_prompt->setPlaceholderText("Type about setup...");
    m_prompt->setFixedHeight(100);
    cardLayout->addWidget(m_prompt);

    auto *buttonsRow = new QHBoxLayout();
    buttonsRow->setSpacing(12);
    buttonsRow->addWidget(m_generateBtn, 2);
    buttonsRow->addWidget(m_executeBtn, 1);

    auto *ideBox = new QHBoxLayout();
    ideBox->setSpacing(8);
    auto *ideLabel = new QLabel("IDE:");
    ideBox->addWidget(ideLabel);
    m_ideSelect->addItems({"Auto", "VS Code", "IntelliJ", "PyCharm"});
    const auto saved = m_ideLauncher->loadPreference();
    const int savedIndex = static_cast<int>(saved);
    if (savedIndex >= 0 && savedIndex < m_ideSelect->count()) m_ideSelect->setCurrentIndex(savedIndex);
    ideBox->addWidget(m_ideSelect, 1);
    ideBox->addStretch();

    buttonsRow->addLayout(ideBox, 1);
    cardLayout->addLayout(buttonsRow);

    rootLayout->addWidget(topCard);

    auto *statusRow = new QHBoxLayout();
    statusRow->setSpacing(12);
    statusRow->addWidget(new QLabel("Status:"));
    statusRow->addWidget(m_statusLabel, 1);
    statusRow->addWidget(new QLabel("Command:"));
    statusRow->addWidget(m_commandLabel, 3);
    rootLayout->addLayout(statusRow);

    auto *splitter = new QSplitter(Qt::Horizontal, central);
    splitter->addWidget(m_tree);
    splitter->addWidget(m_logs);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    rootLayout->addWidget(splitter, 1);

    setCentralWidget(central);
    setWindowTitle("DevOS Desktop");
    resize(1100, 700);
}

void MainWindow::onGenerateClicked() {
    m_plan = QJsonObject();
    m_tree->clear();
    m_logs->clear();
    m_executionId.clear();
    m_ideLaunched = false;
    m_pollTimer->stop();
    m_wsClient->disconnectFrom();
    setStatusText("Generating plan...");
    setCommandText("");

    const auto promptText = m_prompt->toPlainText().trimmed();
    if (promptText.isEmpty()) {
        QMessageBox::warning(this, "Missing prompt", "Please enter a prompt.");
        setStatusText("Idle");
        return;
    }

    m_apiClient->generatePlan(promptText);
}

void MainWindow::onExecuteClicked() {
    if (m_plan.isEmpty()) {
        QMessageBox::information(this, "No plan", "Generate a plan first.");
        return;
    }

    // Ask user where to create the project
    const QString folder = QFileDialog::getExistingDirectory(this, "Select target folder", m_workspacePath,
                                                             QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (folder.isEmpty()) return;

    m_workspacePath = folder;

    const auto validation = SandboxValidator::validatePlan(m_workspacePath, m_plan);
    if (!validation.allowed) {
        QMessageBox::warning(this, "Sandbox Validation Failed", validation.errors.join("\n"));
        return;
    }

    // Prepare local execution: create folders and files, then run commands locally
    m_logs->clear();
    m_executionId.clear();
    m_ideLaunched = false;
    setStatusText("Starting local execution...");
    setCommandText("");

    // Determine project root: if folder not empty and scaffolding commands expect an empty dir,
    // create a subfolder named after the project and run there to avoid conflicts.
    QString projectName = m_plan.value("project_name").toString();
    QDir baseDir(folder);
    const bool folderNotEmpty = !baseDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty();
    bool movedToSubdir = false;
    QString projectRoot = folder;

    // Detect scaffolders that require an empty dir
    const auto cmdsArr = m_plan.value("commands").toArray();
    bool hasEmptyDirScaffold = false;
    for (const auto &c : cmdsArr) {
        const QString s = c.toString().toLower();
        if (s.contains("create-react-app .") || s.contains("npm create vite@latest .") || s.contains("npx create") || s.contains("django-admin startproject") || s.contains("flutter create .")) {
            hasEmptyDirScaffold = true;
            break;
        }
    }

    if (folderNotEmpty && hasEmptyDirScaffold) {
        // create subfolder and run scaffolding there
        projectRoot = baseDir.absoluteFilePath(projectName);
        QDir().mkpath(projectRoot);
        movedToSubdir = true;
        m_logs->appendLog(QStringLiteral("Target folder not empty — running scaffolder inside: %1").arg(projectRoot));
    }

    // Create folders and files only when we are NOT invoking a scaffolder
    if (!hasEmptyDirScaffold) {
        // Create folders inside chosen project root
        const auto folders = m_plan.value("folders").toArray();
        for (const auto &f : folders) {
            const QString rel = f.toString();
            if (rel.isEmpty()) continue;
            QDir dir(projectRoot);
            dir.mkpath(rel);
        }

        // Create files only if content is provided and file does not already exist.
        const auto files = m_plan.value("files").toArray();
        for (const auto &fv : files) {
            const auto obj = fv.toObject();
            const QString path = obj.value("path").toString();
            const QString content = obj.value("content").toString();
            if (path.isEmpty()) continue;
            QDir dir(projectRoot);
            const QString absolute = dir.absoluteFilePath(path);
            QFileInfo fi(absolute);
            QDir().mkpath(fi.path());
            QFile file(absolute);
            if (file.exists()) continue; // don't overwrite existing files
            if (content.isEmpty()) continue; // skip creating empty placeholder files
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                QTextStream out(&file);
                out << content;
                file.close();
            }
        }
    } else {
        m_logs->appendLog("Skipping pre-creation of folders/files because scaffolder will run in an empty subdirectory.");
    }

    // Prepare commands queue and run via cmd /C (Windows)
    const auto cmds = m_plan.value("commands").toArray();
    m_pendingCommands.clear();
    for (const auto &c : cmds) m_pendingCommands << c.toString();

    if (hasEmptyDirScaffold) {
        // Remove any early mkdir/type commands that would create conflicting files/dirs
        QStringList filtered;
        for (const auto &cmd : m_pendingCommands) {
            const QString lower = cmd.trimmed().toLower();
            if (lower.startsWith("mkdir ") || lower.startsWith("type nul >") || lower.startsWith("type ")) {
                // skip these; scaffolder will create the structure
                continue;
            }
            filtered << cmd;
        }
        m_pendingCommands = filtered;
    }

    if (m_pendingCommands.isEmpty()) {
        m_logs->appendLog("No commands to run.");
        // Open IDE on folder
        const auto pref = static_cast<IDELauncher::IDEKind>(m_ideSelect->currentIndex());
        if (!m_ideLauncher->launch(folder, pref)) {
            m_logs->appendLog(QStringLiteral("IDE launch failed. Please open manually."));
        }
        setStatusText("Completed");
        return;
    }

    m_currentCommandIndex = 0;
    if (m_cmdProcess) {
        m_cmdProcess->deleteLater();
        m_cmdProcess = nullptr;
    }
    m_cmdProcess = new QProcess(this);
    connect(m_cmdProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        const QByteArray out = m_cmdProcess->readAllStandardOutput();
        m_logs->appendLog(QString::fromUtf8(out));
    });
    connect(m_cmdProcess, &QProcess::readyReadStandardError, this, [this]() {
        const QByteArray err = m_cmdProcess->readAllStandardError();
        m_logs->appendLog(QString::fromUtf8(err));
    });
    connect(m_cmdProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &MainWindow::onLocalCommandFinished);

    // start first command
    const QString first = m_pendingCommands.at(m_currentCommandIndex);
    setStatusText(QStringLiteral("Running: %1").arg(first));
    setCommandText(first);
    m_cmdProcess->setWorkingDirectory(projectRoot);
    m_cmdProcess->start("cmd", {"/C", first});
}

void MainWindow::onPlanGenerated(const QJsonObject &plan) {
    m_plan = plan;
    m_tree->loadPlan(plan);
    renderCommands(plan.value("commands").toArray());
    setStatusText("Plan ready");
}

void MainWindow::onExecutionStarted(const QString &execId) {
    m_executionId = execId;
    setStatusText("Running...");
    m_pollTimer->start();

    const QUrl wsUrl(QStringLiteral("ws://127.0.0.1:8000/ws/execution/%1").arg(execId));
    m_wsClient->connectTo(wsUrl);
}

void MainWindow::onExecutionStatus(const QJsonObject &status) {
    auto state = status.value("state").toString();
    setStatusText(state);
    if (status.contains("current_command")) {
        setCommandText(status.value("current_command").toString());
    }

    // Append logs if provided
    if (status.contains("logs")) {
        const auto logs = status.value("logs").toArray();
        for (const auto &entry : logs) {
            m_logs->appendLog(entry.toString());
        }
    }

    if (state.compare("completed", Qt::CaseInsensitive) == 0 ||
        state.compare("failed", Qt::CaseInsensitive) == 0) {
        m_pollTimer->stop();
        m_wsClient->disconnectFrom();

        if (!m_ideLaunched && state.compare("completed", Qt::CaseInsensitive) == 0) {
            const auto pref = static_cast<IDELauncher::IDEKind>(m_ideSelect->currentIndex());
            if (!m_ideLauncher->launch(m_workspacePath, pref)) {
                m_logs->appendLog(QStringLiteral("IDE launch failed. Please open manually."));
            } else {
                m_ideLaunched = true;
            }
        }
    }
}

void MainWindow::onNetworkError(const QString &message) {
    m_pollTimer->stop();
    setStatusText("Error");
    QMessageBox::critical(this, "Network Error", message);
}

void MainWindow::onLogReceived(const QString &line) {
    m_logs->appendLog(line);
}

void MainWindow::onSocketStatusUpdated(const QString &status) {
    if (!status.isEmpty()) setStatusText(status);
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_F11) {
        if (isFullScreen()) showNormal(); else showFullScreen();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::pollStatus() {
    if (m_executionId.isEmpty()) return;
    m_apiClient->getExecutionStatus(m_executionId);
}

void MainWindow::setStatusText(const QString &text) {
    m_statusLabel->setText(text);
}

void MainWindow::setCommandText(const QString &text) {
    m_commandLabel->setText(text);
}

void MainWindow::renderCommands(const QJsonArray &commands) {
    m_logs->appendLog(QStringLiteral("=== Commands ==="));
    if (commands.isEmpty()) {
        m_logs->appendLog(QStringLiteral("No commands returned."));
        return;
    }
    int idx = 1;
    for (const auto &cmd : commands) {
        m_logs->appendLog(QStringLiteral("%1. %2").arg(idx++).arg(cmd.toString()));
    }
}

void MainWindow::onIdePreferenceChanged(int index) {
    const auto kind = static_cast<IDELauncher::IDEKind>(index);
    m_ideLauncher->savePreference(kind);
}

void MainWindow::onLocalCommandFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(status)
    const QString lastCmd = m_pendingCommands.value(m_currentCommandIndex);
    m_logs->appendLog(QStringLiteral("Command finished: %1 (exit=%2)").arg(lastCmd).arg(exitCode));
    m_currentCommandIndex++;
    if (m_currentCommandIndex >= m_pendingCommands.size()) {
        setStatusText("Completed");
        setCommandText("");
        // Launch IDE
        const auto pref = static_cast<IDELauncher::IDEKind>(m_ideSelect->currentIndex());
        if (!m_ideLauncher->launch(m_workspacePath, pref)) {
            m_logs->appendLog(QStringLiteral("IDE launch failed. Please open manually."));
        } else {
            m_ideLaunched = true;
        }
        return;
    }

    const QString next = m_pendingCommands.at(m_currentCommandIndex);
    setStatusText(QStringLiteral("Running: %1").arg(next));
    setCommandText(next);
    m_cmdProcess->start("cmd", {"/C", next});
}
