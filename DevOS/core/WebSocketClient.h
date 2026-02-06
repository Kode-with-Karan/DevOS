#pragma once

#include <QObject>
#include <QTimer>
#include <QWebSocket>
#include <QUrl>

class WebSocketClient : public QObject {
    Q_OBJECT
public:
    explicit WebSocketClient(QObject *parent = nullptr);

    void connectTo(const QUrl &url);
    void disconnectFrom();

signals:
    void connected();
    void disconnected();
    void logReceived(const QString &line);
    void statusUpdated(const QString &status);
    void errorOccurred(const QString &msg);

private:
    void scheduleReconnect();
    void handleMessage(const QString &msg);

    QWebSocket m_socket;
    QTimer m_reconnectTimer;
    QUrl m_lastUrl;
    bool m_manualClose{false};
};
