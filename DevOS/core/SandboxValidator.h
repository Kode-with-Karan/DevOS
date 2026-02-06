#pragma once

#include <QDir>
#include <QJsonObject>
#include <QStringList>

struct ValidationResult {
    bool allowed{true};
    QStringList errors;
};

class SandboxValidator {
public:
    static ValidationResult validatePlan(const QString &workspace, const QJsonObject &plan);

private:
    static bool isPathSafe(const QString &workspace, const QString &candidate);
    static bool containsDangerousCommand(const QString &commandLower);
};
