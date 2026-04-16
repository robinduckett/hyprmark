#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace Ipc {

    // Compute the runtime socket path: $XDG_RUNTIME_DIR/hypr/$HIS/hyprmark.sock
    // Falls back to /tmp/hyprmark-<uid>.sock when the runtime dir is unset.
    QString socketPath();

    // Build a line-delimited JSON request (with trailing '\n').
    QByteArray encodeRequest(const QString& cmd, const QStringList& args);

    // Successful/error replies.
    QByteArray okReply();
    QByteArray okReply(const QJsonObject& extra);
    QByteArray errorReply(const QString& message);

    // Parse one request line. Returns an empty object on malformed input.
    QJsonObject parseLine(const QByteArray& line);

} // namespace Ipc
