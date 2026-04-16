#pragma once

#include "../defines.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QLocalServer;
class QLocalSocket;
class CMainWindow;

// CIpcServer listens on $XDG_RUNTIME_DIR/hypr/$HIS/hyprmark.sock and routes
// line-delimited JSON commands to methods on the active main window / theme
// manager. The target window is resolved on each call via
// g_pHyprmark->activeWindow() so IPC naturally follows focus.
class CIpcServer : public QObject {
    Q_OBJECT

  public:
    explicit CIpcServer(QObject* parent = nullptr);
    ~CIpcServer() override;

    // Unlinks any stale socket at the target path then begins listening.
    // Returns true on success.
    bool listen();

  private slots:
    void onNewConnection();
    void onReadyRead(QLocalSocket* client);

  private:
    // Dispatch the decoded command. Returns a single-line JSON reply (no '\n').
    QByteArray handle(const QString& cmd, const QJsonArray& args);

    QLocalServer* m_server = nullptr;
};

inline UP<CIpcServer> g_pIpcServer;
