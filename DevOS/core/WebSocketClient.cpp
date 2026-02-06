#include "WebSocketClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>

static constexpr int kReconnectDelayMs = 1500;

WebSocketClient::WebSocketClient(QObject *parent) : QObject(parent) {
    m_reconnectTimer.setSingleShot(true);

    connect(&m_socket, &QWebSocket::connected, this, &WebSocketClient::connected);
    connect(&m_socket, &QWebSocket::disconnected, this, [this]() {
        emit disconnected();
        if (!m_manualClose && m_lastUrl.isValid()) scheduleReconnect();
    });
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &WebSocketClient::handleMessage);
    connect(&m_socket, &QWebSocket::errorOccurred, this, [this]() {
        emit errorOccurred(m_socket.errorString());
        if (!m_manualClose && m_lastUrl.isValid()) scheduleReconnect();
    });
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (!m_manualClose && m_lastUrl.isValid()) m_socket.open(m_lastUrl);
    });
}

void WebSocketClient::connectTo(const QUrl &url) {
    m_manualClose = false;
    m_lastUrl = url;
    m_reconnectTimer.stop();
    m_socket.open(url);
}

void WebSocketClient::disconnectFrom() {
    m_manualClose = true;
    m_reconnectTimer.stop();
    m_socket.close();
}

void WebSocketClient::scheduleReconnect() {
    if (!m_reconnectTimer.isActive()) {
        m_reconnectTimer.start(kReconnectDelayMs);
    }
}

void WebSocketClient::handleMessage(const QString &msg) {
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(msg.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        emit errorOccurred(QStringLiteral("Invalid JSON: %1").arg(parseError.errorString()));
        return;
    }

    const auto obj = doc.object();
    const auto status = obj.value("status").toString();
    if (!status.isEmpty()) emit statusUpdated(status);

    if (obj.value("type").toString() == QStringLiteral("log")) {
        const auto command = obj.value("command").toString();
        const auto stdoutText = obj.value("stdout").toString();
        const auto stderrText = obj.value("stderr").toString();

        QStringList lines;
        if (!status.isEmpty() || !command.isEmpty()) {
            lines << QStringLiteral("[%1] %2").arg(status, command);
        }
        if (!stdoutText.isEmpty()) lines << stdoutText;
        if (!stderrText.isEmpty()) lines << QStringLiteral("stderr: %1").arg(stderrText);

        const auto joined = lines.join('\n');
        if (!joined.isEmpty()) emit logReceived(joined);
    }
}
