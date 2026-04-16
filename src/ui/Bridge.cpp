#include "Bridge.hpp"

#include "../helpers/Log.hpp"

#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QUrl>

CBridge::CBridge(QObject* parent) : QObject(parent) {}

void CBridge::copyToClipboard(const QString& text) {
    QGuiApplication::clipboard()->setText(text);
    Debug::log(TRACE, "copyToClipboard: {} chars", text.size());
}

void CBridge::openExternal(const QString& url) {
    const QUrl u(url);
    if (u.scheme() == QLatin1String("http") || u.scheme() == QLatin1String("https") || u.scheme() == QLatin1String("mailto")) {
        Debug::log(LOG, "openExternal: {}", url.toStdString());
        QDesktopServices::openUrl(u);
        emit externalLinkRequested(url);
    } else
        Debug::log(WARN, "openExternal rejected non-web scheme: {}", url.toStdString());
}

void CBridge::reportHeadings(const QString& json) {
    emit headingsReported(json);
}
