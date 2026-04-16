#include "IpcProtocol.hpp"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QString>

#include <unistd.h>

namespace Ipc {

    QString socketPath() {
        const QByteArray xdg = qgetenv("XDG_RUNTIME_DIR");
        const QByteArray his = qgetenv("HYPRLAND_INSTANCE_SIGNATURE");
        if (!xdg.isEmpty() && !his.isEmpty()) {
            const QString dir = QString::fromLocal8Bit(xdg) + QStringLiteral("/hypr/") + QString::fromLocal8Bit(his);
            QDir().mkpath(dir);
            return dir + QStringLiteral("/hyprmark.sock");
        }
        // Fallback: /tmp/hyprmark-<uid>.sock — not inside a $HIS-keyed dir but
        // still user-scoped via the uid suffix.
        return QStringLiteral("/tmp/hyprmark-%1.sock").arg(static_cast<int>(::geteuid()));
    }

    QByteArray encodeRequest(const QString& cmd, const QStringList& args) {
        QJsonObject o;
        o.insert("cmd", cmd);
        QJsonArray a;
        for (const auto& s : args) a.append(s);
        o.insert("args", a);
        QByteArray out = QJsonDocument(o).toJson(QJsonDocument::Compact);
        out.append('\n');
        return out;
    }

    QByteArray okReply() {
        return okReply(QJsonObject{});
    }

    QByteArray okReply(const QJsonObject& extra) {
        QJsonObject o = extra;
        o.insert("ok", true);
        QByteArray out = QJsonDocument(o).toJson(QJsonDocument::Compact);
        out.append('\n');
        return out;
    }

    QByteArray errorReply(const QString& message) {
        QJsonObject o;
        o.insert("ok", false);
        o.insert("error", message);
        QByteArray out = QJsonDocument(o).toJson(QJsonDocument::Compact);
        out.append('\n');
        return out;
    }

    QJsonObject parseLine(const QByteArray& line) {
        QJsonParseError err{};
        const auto doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
            return {};
        return doc.object();
    }

} // namespace Ipc
