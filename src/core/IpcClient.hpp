#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace Ipc {

    // Try to send a command to a running hyprmark instance. Returns:
    //   * empty optional if no server is listening (caller should become the server)
    //   * the raw reply line on success (including error replies)
    //
    // Blocking, with a small timeout — used from main() before Qt event loop
    // is running.
    struct ClientResult {
        bool       connected = false;
        bool       ok        = false;
        QByteArray reply;
    };

    ClientResult sendCommand(const QString& cmd, const QStringList& args, int connectTimeoutMs = 200, int replyTimeoutMs = 500);

} // namespace Ipc
