#include "IpcClient.hpp"

#include "IpcProtocol.hpp"

#include <QLocalSocket>

namespace Ipc {

    ClientResult sendCommand(const QString& cmd, const QStringList& args, int connectTimeoutMs, int replyTimeoutMs) {
        ClientResult res;

        QLocalSocket s;
        s.connectToServer(socketPath());
        if (!s.waitForConnected(connectTimeoutMs))
            return res; // no server — caller becomes one

        res.connected = true;
        const auto payload = encodeRequest(cmd, args);
        s.write(payload);
        s.flush();

        if (!s.waitForReadyRead(replyTimeoutMs)) {
            res.reply = errorReply(QStringLiteral("timeout"));
            return res;
        }

        res.reply = s.readLine();
        const auto obj = parseLine(res.reply);
        res.ok = obj.value("ok").toBool(false);
        s.disconnectFromServer();
        return res;
    }

} // namespace Ipc
