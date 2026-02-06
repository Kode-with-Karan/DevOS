#include "ApiClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QTimer>

static constexpr int kDefaultTimeoutMs = 120000; // allow long LLM responses

ApiClient::ApiClient(QObject *parent) : QObject(parent), m_baseUrl("http://127.0.0.1:8000") {}

void ApiClient::setBaseUrl(const QUrl &url) {
    m_baseUrl = url;
}

QNetworkRequest ApiClient::makeJsonRequest(const QUrl &path) const {
    QNetworkRequest req(path);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(kDefaultTimeoutMs);
    return req;
}

void ApiClient::handleError(const QString &context, QNetworkReply *reply) {
    const auto statusAttr = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    const int status = statusAttr.isValid() ? statusAttr.toInt() : 0;

    QByteArray body;
    if (reply->isReadable()) {
        body = reply->readAll();
    }

    const QString bodyText = QString::fromUtf8(body);
    const QString errText = reply->errorString();

    if (status > 0) {
        emit networkError(QStringLiteral("%1 (HTTP %2): %3 | %4").arg(context).arg(status).arg(bodyText, errText));
    } else {
        emit networkError(QStringLiteral("%1: %2 | %3").arg(context, errText, bodyText));
    }
}

void ApiClient::generatePlan(const QString &prompt) {
    QJsonObject payload{{"prompt", prompt}};
    auto url = m_baseUrl.resolved(QUrl("/generate-plan"));
    auto *reply = m_network.post(makeJsonRequest(url), QJsonDocument(payload).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            handleError("Generate plan failed", reply);
            return;
        }
        const auto data = reply->readAll();
        const auto doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            emit networkError(QStringLiteral("Generate plan failed: invalid JSON response"));
            return;
        }
        emit planGenerated(doc.object());
    });
}

void ApiClient::executePlan(const QJsonObject &plan) {
    auto url = m_baseUrl.resolved(QUrl("/execute-plan"));
    auto *reply = m_network.post(makeJsonRequest(url), QJsonDocument(plan).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            handleError("Execute plan failed", reply);
            return;
        }
        const auto data = reply->readAll();
        const auto doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            emit networkError(QStringLiteral("Execute plan failed: invalid JSON response"));
            return;
        }
        const auto execId = doc.object().value("execution_id").toString();
        emit executionStarted(execId);
    });
}

void ApiClient::getExecutionStatus(const QString &executionId) {
    auto url = m_baseUrl.resolved(QUrl(QString("/execution/%1").arg(executionId)));
    auto *reply = m_network.get(makeJsonRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            handleError("Execution status failed", reply);
            return;
        }
        const auto data = reply->readAll();
        const auto doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            emit networkError(QStringLiteral("Execution status failed: invalid JSON response"));
            return;
        }
        emit executionStatusUpdated(doc.object());
    });
}
