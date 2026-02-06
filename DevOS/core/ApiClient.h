#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>

class ApiClient : public QObject {
    Q_OBJECT
public:
    explicit ApiClient(QObject *parent = nullptr);

    void setBaseUrl(const QUrl &url);

    void generatePlan(const QString &prompt);
    void executePlan(const QJsonObject &plan);
    void getExecutionStatus(const QString &executionId);

signals:
    void planGenerated(const QJsonObject &plan);
    void executionStarted(const QString &executionId);
    void executionStatusUpdated(const QJsonObject &status);
    void networkError(const QString &message);

private:
    QNetworkRequest makeJsonRequest(const QUrl &path) const;
    void handleError(const QString &context, QNetworkReply *reply);

    QNetworkAccessManager m_network;
    QUrl m_baseUrl;
};
