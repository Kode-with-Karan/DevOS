#include "IDELauncher.h"

#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace {
QString kindToString(IDELauncher::IDEKind kind) {
    switch (kind) {
    case IDELauncher::IDEKind::VSCode: return QStringLiteral("vscode");
    case IDELauncher::IDEKind::IntelliJ: return QStringLiteral("intellij");
    case IDELauncher::IDEKind::PyCharm: return QStringLiteral("pycharm");
    default: return QStringLiteral("auto");
    }
}

IDELauncher::IDEKind stringToKind(const QString &s) {
    const auto lower = s.toLower();
    if (lower == "vscode") return IDELauncher::IDEKind::VSCode;
    if (lower == "intellij") return IDELauncher::IDEKind::IntelliJ;
    if (lower == "pycharm") return IDELauncher::IDEKind::PyCharm;
    return IDELauncher::IDEKind::Auto;
}
}

IDELauncher::IDELauncher(QObject *parent) : QObject(parent) {}

QString IDELauncher::resolveCommandFor(IDEKind kind) const {
    switch (kind) {
    case IDEKind::VSCode: return QStringLiteral("code");
    case IDEKind::IntelliJ: return QStringLiteral("idea64");
    case IDEKind::PyCharm: return QStringLiteral("pycharm64");
    case IDEKind::Auto: break;
    }
    return {};
}

bool IDELauncher::isAvailable(const QString &command) const {
    if (command.isEmpty()) return false;
    QProcess proc;
    proc.start(QStringLiteral("where"), {command});
    proc.waitForFinished(1500);
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

IDELauncher::IDEKind IDELauncher::fallbackDetected() const {
    if (isAvailable("code")) return IDEKind::VSCode;
    if (isAvailable("idea64") || isAvailable("idea")) return IDEKind::IntelliJ;
    if (isAvailable("pycharm64") || isAvailable("pycharm")) return IDEKind::PyCharm;
    return IDEKind::Auto;
}

QStringList IDELauncher::availableIDEs() const {
    QStringList found;
    if (isAvailable("code")) found << "VS Code";
    if (isAvailable("idea64") || isAvailable("idea")) found << "IntelliJ";
    if (isAvailable("pycharm64") || isAvailable("pycharm")) found << "PyCharm";
    return found;
}

bool IDELauncher::launch(const QString &projectPath, IDEKind preferred) {
    IDEKind toLaunch = preferred;
    if (toLaunch == IDEKind::Auto) toLaunch = loadPreference();
    if (toLaunch == IDEKind::Auto) toLaunch = fallbackDetected();

    const auto command = resolveCommandFor(toLaunch);
    if (command.isEmpty() || !isAvailable(command)) {
        emit launchFailed(QStringLiteral("No IDE available (tried preference %1)").arg(kindToString(toLaunch)));
        return false;
    }

    const bool started = QProcess::startDetached(command, {projectPath});
    if (!started) {
        emit launchFailed(QStringLiteral("Failed to launch %1 for path: %2").arg(kindToString(toLaunch), projectPath));
    }
    return started;
}

QString IDELauncher::preferenceFilePath() const {
    const auto dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + "/devos_ide.json";
}

IDELauncher::IDEKind IDELauncher::loadPreference() const {
    QFile file(preferenceFilePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) return IDEKind::Auto;
    const auto doc = QJsonDocument::fromJson(file.readAll());
    const auto obj = doc.object();
    return stringToKind(obj.value(QStringLiteral("preferred_ide")).toString());
}

void IDELauncher::savePreference(IDEKind kind) const {
    QFile file(preferenceFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    QJsonObject obj;
    obj.insert(QStringLiteral("preferred_ide"), kindToString(kind));
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}
