#include "FileOpenRouter.hpp"

#include "../helpers/Log.hpp"

#include <QEvent>
#include <QFileOpenEvent>
#include <QUrl>

CFileOpenRouter::CFileOpenRouter(Handler handler, QObject* parent) : QObject(parent), m_handler(std::move(handler)) {}

bool CFileOpenRouter::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() != QEvent::FileOpen)
        return QObject::eventFilter(watched, event);

    auto* fo = static_cast<QFileOpenEvent*>(event);

    // file() is already the local path for file:// URLs; for anything else
    // (custom schemes, http) it is empty and we have nothing to render.
    const QString path = fo->file();
    if (path.isEmpty()) {
        Debug::log(WARN, "ignoring non-file open request: {}", fo->url().toString().toStdString());
        event->accept();
        return true;
    }

    Debug::log(LOG, "system open request: {}", path.toStdString());
    if (m_handler)
        m_handler(path.toStdString());
    event->accept();
    return true;
}
