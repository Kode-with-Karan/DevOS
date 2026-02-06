#include "SandboxValidator.h"

#include <QJsonArray>

namespace {
constexpr int kMaxFiles = 200;
constexpr int kMaxCommands = 1000;
constexpr int kMaxFileBytes = 1 * 1024 * 1024; // 1MB

QStringList collectCommands(const QJsonObject &plan) {
    QStringList commands;
    const auto tryArray = [&](const QJsonValue &val) {
        if (!val.isArray()) return;
        for (const auto &entry : val.toArray()) {
            if (entry.isString()) {
                commands << entry.toString();
            } else if (entry.isObject()) {
                const auto obj = entry.toObject();
                if (obj.contains("command")) commands << obj.value("command").toString();
                else if (obj.contains("cmd")) commands << obj.value("cmd").toString();
            }
        }
    };

    if (plan.contains("commands")) tryArray(plan.value("commands"));
    if (plan.contains("steps")) tryArray(plan.value("steps"));
    return commands;
}
}

ValidationResult SandboxValidator::validatePlan(const QString &workspace, const QJsonObject &plan) {
    ValidationResult result;

    const auto files = plan.value("files").toArray();
    if (files.size() > kMaxFiles) {
        result.allowed = false;
        result.errors << QStringLiteral("Too many files (%1 > %2)").arg(files.size()).arg(kMaxFiles);
    }

    for (const auto &f : files) {
        if (!f.isObject()) continue;
        const auto obj = f.toObject();
        const auto path = obj.value("path").toString();
        if (path.isEmpty()) {
            result.allowed = false;
            result.errors << QStringLiteral("File entry missing path");
            continue;
        }
        if (QDir::isAbsolutePath(path) || path.startsWith('/') || path.contains(":\\")) {
            result.allowed = false;
            result.errors << QStringLiteral("Absolute paths not allowed: %1").arg(path);
        }
        if (path.contains("..")) {
            result.allowed = false;
            result.errors << QStringLiteral("Path traversal not allowed: %1").arg(path);
        }
        if (!isPathSafe(workspace, path)) {
            result.allowed = false;
            result.errors << QStringLiteral("Path escapes workspace: %1").arg(path);
        }

        const auto content = obj.value("content").toString();
        if (!content.isEmpty() && content.toUtf8().size() > kMaxFileBytes) {
            result.allowed = false;
            result.errors << QStringLiteral("File too large (%1 bytes): %2")
                                   .arg(content.toUtf8().size())
                                   .arg(path);
        }
    }

    const auto commands = collectCommands(plan);
    if (commands.size() > kMaxCommands) {
        result.allowed = false;
        result.errors << QStringLiteral("Too many commands (%1 > %2)").arg(commands.size()).arg(kMaxCommands);
    }

    for (const auto &cmd : commands) {
        const auto lower = cmd.toLower();
        if (containsDangerousCommand(lower)) {
            result.allowed = false;
            result.errors << QStringLiteral("Dangerous command detected: %1").arg(cmd);
        }
    }

    return result;
}

bool SandboxValidator::isPathSafe(const QString &workspace, const QString &candidate) {
    QDir base(workspace);
    const QString abs = QDir(workspace).absoluteFilePath(candidate);
    return QDir::cleanPath(abs).startsWith(QDir::cleanPath(base.absolutePath()));
}

bool SandboxValidator::containsDangerousCommand(const QString &commandLower) {
    static const QStringList blacklist = {
        "format",
        "shutdown",
        "rd /s",
        "del c:\\",
        "powershell -executionpolicy",
        "reg add",
        "taskkill",
        "net user",
        "rm -rf /",
        "mkfs",
        "diskpart",
        "sc delete",
        "bcdedit",
        "vssadmin delete shadows",
        "cipher /w",
        "takeown",
        "icacls /grant",
        "sudo",
        "chmod 777 /",
        "chown root",
        "apt-get remove",
        "yum remove"
    };

    for (const auto &token : blacklist) {
        if (commandLower.contains(token)) return true;
    }
    return false;
}
