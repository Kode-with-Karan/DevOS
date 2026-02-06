#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>

class IDELauncher : public QObject {
    Q_OBJECT
public:
    enum class IDEKind { Auto, VSCode, IntelliJ, PyCharm };

    explicit IDELauncher(QObject *parent = nullptr);

    bool launch(const QString &projectPath, IDEKind preferred = IDEKind::Auto);
    QStringList availableIDEs() const; // human-readable list of detected IDE names

    IDEKind loadPreference() const;
    void savePreference(IDEKind kind) const;

signals:
    void launchFailed(const QString &message);

private:
    bool isAvailable(const QString &command) const;
    QString resolveCommandFor(IDEKind kind) const;
    IDEKind fallbackDetected() const;
    QString preferenceFilePath() const;
};
