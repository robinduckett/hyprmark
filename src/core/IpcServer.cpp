#include "IpcServer.hpp"

#include "../helpers/Log.hpp"
#include "../render/ThemeManager.hpp"
#include "../ui/MainWindow.hpp"
#include "../ui/RenderView.hpp"
#include "IpcProtocol.hpp"
#include "hyprmark.hpp"

#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>

#include <filesystem>

CIpcServer::CIpcServer(QObject* parent) : QObject(parent) {}

CIpcServer::~CIpcServer() {
    if (m_server) {
        m_server->close();
        QLocalServer::removeServer(m_server->fullServerName());
    }
}

bool CIpcServer::listen() {
    const QString path = Ipc::socketPath();
    // Ensure no stale socket blocks us.
    QLocalServer::removeServer(path);

    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(path)) {
        Debug::log(ERR, "IPC listen failed at {}: {}", path.toStdString(), m_server->errorString().toStdString());
        return false;
    }
    connect(m_server, &QLocalServer::newConnection, this, &CIpcServer::onNewConnection);
    Debug::log(LOG, "IPC listening on {}", path.toStdString());
    return true;
}

void CIpcServer::onNewConnection() {
    while (auto* client = m_server->nextPendingConnection()) {
        connect(client, &QLocalSocket::readyRead, this, [this, client]() { onReadyRead(client); });
        connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
    }
}

void CIpcServer::onReadyRead(QLocalSocket* client) {
    while (client->canReadLine()) {
        const auto line    = client->readLine();
        const auto obj     = Ipc::parseLine(line.trimmed());
        if (obj.isEmpty()) {
            client->write(Ipc::errorReply(QStringLiteral("malformed JSON")));
            client->flush();
            continue;
        }
        const auto cmd   = obj.value("cmd").toString();
        const auto args  = obj.value("args").toArray();
        client->write(handle(cmd, args));
        client->flush();
    }
}

QByteArray CIpcServer::handle(const QString& cmd, const QJsonArray& args) {
    auto stringAt = [&](int i) -> QString {
        if (i < 0 || i >= args.size()) return {};
        return args.at(i).toString();
    };

    // Resolve the active window now rather than when the queued lambda runs —
    // captures stay valid even if focus changes between the reply and the
    // actual invocation.
    CMainWindow* const target = g_pHyprmark ? g_pHyprmark->activeWindow() : nullptr;

    // `open` is special: it always spawns a new window (matches the external
    // `hyprmark path.md` behaviour).
    if (cmd == QLatin1String("open")) {
        const auto path = stringAt(0);
        if (path.isEmpty()) return Ipc::errorReply(QStringLiteral("open requires a path"));
        QMetaObject::invokeMethod(qApp, [path]() {
            if (g_pHyprmark) g_pHyprmark->newWindow(path.toStdString());
        }, Qt::QueuedConnection);
        return Ipc::okReply();
    }

    // `open-here` swaps the active window's content (equivalent to Ctrl+O).
    if (cmd == QLatin1String("open-here")) {
        const auto path = stringAt(0);
        if (path.isEmpty()) return Ipc::errorReply(QStringLiteral("open-here requires a path"));
        if (!target) return Ipc::errorReply(QStringLiteral("no active window"));
        QMetaObject::invokeMethod(target, [target, path]() {
            target->openFile(path.toStdString());
            target->raise();
            target->activateWindow();
        }, Qt::QueuedConnection);
        return Ipc::okReply();
    }

    if (cmd == QLatin1String("raise")) {
        if (!target) {
            // No windows yet — spawn a fresh empty-state one.
            QMetaObject::invokeMethod(qApp, []() {
                if (g_pHyprmark) g_pHyprmark->newWindow({});
            }, Qt::QueuedConnection);
            return Ipc::okReply();
        }
        QMetaObject::invokeMethod(target, [target]() {
            target->show();
            target->raise();
            target->activateWindow();
        }, Qt::QueuedConnection);
        return Ipc::okReply();
    }

    if (cmd == QLatin1String("list-themes")) {
        QJsonArray arr;
        if (g_pThemeManager) {
            for (const auto& t : g_pThemeManager->listThemes())
                arr.append(QString::fromStdString(t.name));
        }
        QJsonObject extra;
        extra.insert("themes", arr);
        return Ipc::okReply(extra);
    }

    // Everything else targets the active window.
    if (!target)
        return Ipc::errorReply(QStringLiteral("no active window"));

    if (cmd == QLatin1String("theme")) {
        const auto name = stringAt(0);
        if (name.isEmpty()) return Ipc::errorReply(QStringLiteral("theme requires a name"));
        QMetaObject::invokeMethod(target, [target, name]() { target->applyTheme(name); }, Qt::QueuedConnection);
        return Ipc::okReply();
    }

    if (cmd == QLatin1String("cycle-theme")) {
        QMetaObject::invokeMethod(target, [target]() { target->cycleTheme(); }, Qt::QueuedConnection);
        return Ipc::okReply();
    }

    if (cmd == QLatin1String("toggle-toc")) {
        QMetaObject::invokeMethod(target, [target]() { target->toggleToc(); }, Qt::QueuedConnection);
        return Ipc::okReply();
    }

    if (cmd == QLatin1String("zoom")) {
        const auto what = stringAt(0);
        if (what != QLatin1String("in") && what != QLatin1String("out") && what != QLatin1String("reset"))
            return Ipc::errorReply(QStringLiteral("zoom expects in|out|reset"));
        QMetaObject::invokeMethod(target, [target, what]() {
            if (what == QLatin1String("in"))         target->zoomIn();
            else if (what == QLatin1String("out"))   target->zoomOut();
            else if (what == QLatin1String("reset")) target->zoomReset();
        }, Qt::QueuedConnection);
        return Ipc::okReply();
    }

    if (cmd == QLatin1String("find")) {
        QMetaObject::invokeMethod(target, [target]() { target->findInPage(); }, Qt::QueuedConnection);
        return Ipc::okReply();
    }

    if (cmd == QLatin1String("reload")) {
        QMetaObject::invokeMethod(target, "onSourceFileChanged", Qt::QueuedConnection);
        return Ipc::okReply();
    }

    if (cmd == QLatin1String("export-pdf")) {
        const auto out = stringAt(0);
        QMetaObject::invokeMethod(target, [target, out]() {
            if (out.isEmpty())
                target->exportPdf();
            else if (auto* rv = target->renderView())
                rv->page()->printToPdf(out);
        }, Qt::QueuedConnection);
        return Ipc::okReply();
    }

    if (cmd == QLatin1String("close")) {
        QMetaObject::invokeMethod(target, "close", Qt::QueuedConnection);
        return Ipc::okReply();
    }

    return Ipc::errorReply(QStringLiteral("unknown command: ") + cmd);
}
