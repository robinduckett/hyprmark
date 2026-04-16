#include "UrlInterceptor.hpp"

#include "../helpers/Log.hpp"

CUrlInterceptor::CUrlInterceptor(QObject* parent) : QWebEngineUrlRequestInterceptor(parent) {}

void CUrlInterceptor::interceptRequest(QWebEngineUrlRequestInfo& info) {
    const QString scheme = info.requestUrl().scheme();

    if (scheme == QLatin1String("data") || scheme == QLatin1String("file") || scheme == QLatin1String("qrc"))
        return; // allow

    if (scheme == QLatin1String("http") || scheme == QLatin1String("https")) {
        if (m_allowRemote.load(std::memory_order_relaxed))
            return;

        // block + count + signal
        info.block(true);
        const int prev = m_blocked.fetch_add(1, std::memory_order_relaxed);
        if (prev == 0) // only emit once per reset
            emit remoteBlocked();
        return;
    }

    // everything else (ftp://, gopher://, chrome://, etc.): block silently
    info.block(true);
}

void CUrlInterceptor::setAllowRemote(bool allow) {
    m_allowRemote.store(allow, std::memory_order_relaxed);
}

bool CUrlInterceptor::allowRemote() const {
    return m_allowRemote.load(std::memory_order_relaxed);
}

void CUrlInterceptor::resetBlockedCount() {
    m_blocked.store(0, std::memory_order_relaxed);
}

int CUrlInterceptor::blockedCount() const {
    return m_blocked.load(std::memory_order_relaxed);
}
