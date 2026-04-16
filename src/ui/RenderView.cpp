#include "RenderView.hpp"

#include "../helpers/Log.hpp"
#include "Bridge.hpp"
#include "UrlInterceptor.hpp"

#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonValue>
#include <QWebChannel>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEngineNavigationRequest>
#include <QWebEngineSettings>

namespace {
    // A QWebEnginePage that forwards every JS console.log / error to stderr
    // via Debug::log — essential for diagnosing issues in the embedded page.
    class CLoggingPage : public QWebEnginePage {
      public:
        using QWebEnginePage::QWebEnginePage;
        void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                      const QString& message,
                                      int lineNumber,
                                      const QString& sourceID) override {
            const char* lvl = (level == ErrorMessageLevel) ? "ERR"
                            : (level == WarningMessageLevel) ? "WARN"
                            : "LOG";
            Debug::log(LOG, "[webview:{}] {}:{} — {}", lvl, sourceID.toStdString(), lineNumber, message.toStdString());
        }
    };
}

CRenderView::CRenderView(QWidget* parent) : QWebEngineView(parent) {
    // Qt6's defaultProfile is already off-the-record (no cookies / history /
    // cache).
    auto* profile = QWebEngineProfile::defaultProfile();

    // Chromium's own drag-and-drop would normally navigate to dropped files.
    // We want drops to reach the main window so openFile() runs instead.
    setAcceptDrops(false);

    // Swap in our logging page so console.log from hyprmark.js surfaces.
    auto* loggingPage = new CLoggingPage(profile, this);
    setPage(loggingPage);

    // Attach our URL interceptor to THIS page (not the profile). That way
    // each CRenderView in a multi-window setup has its own interceptor,
    // and each CMainWindow sees only events from its own page.
    m_interceptor = new CUrlInterceptor(this);
    loggingPage->setUrlRequestInterceptor(m_interceptor);

    auto* s = settings();
    s->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls,   true);
    // Let remote URLs reach the interceptor. The interceptor is the single
    // source of truth for the allow/block decision (see CUrlInterceptor).
    s->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    s->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard,    true);
    s->setAttribute(QWebEngineSettings::ErrorPageEnabled,                false);
    s->setAttribute(QWebEngineSettings::PdfViewerEnabled,                false);

    // QWebChannel + Bridge — this is how C++ talks to hyprmark.js.
    m_bridge  = new CBridge(this);
    m_channel = new QWebChannel(this);
    m_channel->registerObject(QStringLiteral("hyprmarkHost"), m_bridge);
    page()->setWebChannel(m_channel);

    // Wire C++ signals on CBridge → JS listeners via the channel. The slots
    // below will be invoked on the JS side (HM.swapContent etc.) since those
    // functions connect to these signals in hyprmark.js.
    // (hyprmark.js currently exposes these as HM.swapContent on window, but
    // we wire them via direct QObject::connect from our explicit methods
    // below instead of via JS-side signal subscription. This keeps the JS
    // entry-point count minimal.)

    // Link clicks for http(s) → open in system browser, not in-view.
    connect(page(), &QWebEnginePage::navigationRequested, this,
            [this](QWebEngineNavigationRequest& req) {
                const auto scheme = req.url().scheme();
                if (req.navigationType() == QWebEngineNavigationRequest::LinkClickedNavigation
                    && (scheme == QLatin1String("http") || scheme == QLatin1String("https"))) {
                    req.reject();
                    QDesktopServices::openUrl(req.url());
                }
            });
}

CRenderView::~CRenderView() = default;

void CRenderView::loadInitial(const QString& fullPageHtml, const QUrl& baseUrl) {
    // setContent bypasses the 2 MB limit of setHtml (which encodes via data:).
    page()->setContent(fullPageHtml.toUtf8(), QStringLiteral("text/html;charset=utf-8"), baseUrl);
    m_interceptor->resetBlockedCount();
}

// JSON-encode a single string the same way JS.stringify would — used for
// passing HTML/path/url arguments into runJavaScript safely.
static QString jsString(const QString& s) {
    QString o;
    o.reserve(s.size() + 2);
    o.append('"');
    for (QChar c : s) {
        const auto u = c.unicode();
        if (u == '"' || u == '\\')      o.append(QChar('\\')).append(c);
        else if (u == '\n')             o.append(QStringLiteral("\\n"));
        else if (u == '\r')             o.append(QStringLiteral("\\r"));
        else if (u == '\t')             o.append(QStringLiteral("\\t"));
        else if (u == 0x2028)           o.append(QStringLiteral("\\u2028"));
        else if (u == 0x2029)           o.append(QStringLiteral("\\u2029"));
        else if (u < 0x20)              o.append(QStringLiteral("\\u%1").arg(static_cast<int>(u), 4, 16, QChar('0')));
        else                            o.append(c);
    }
    o.append('"');
    return o;
}

void CRenderView::swapBody(const QString& bodyHtml, const QString& sourcePath) {
    m_interceptor->resetBlockedCount();
    const QString js = QStringLiteral("window.hyprmark && window.hyprmark.swapContent && window.hyprmark.swapContent(%1, %2);")
                           .arg(jsString(bodyHtml), jsString(sourcePath));
    page()->runJavaScript(js);
}

void CRenderView::swapTheme(const QString& themeUrl) {
    const QString js = QStringLiteral("window.hyprmark && window.hyprmark.swapTheme && window.hyprmark.swapTheme(%1);")
                           .arg(jsString(themeUrl));
    page()->runJavaScript(js);
}

void CRenderView::setScale(double factor) {
    const QString js = QStringLiteral("window.hyprmark && window.hyprmark.setScale && window.hyprmark.setScale(%1);").arg(factor);
    page()->runJavaScript(js);
}

void CRenderView::scrollToAnchor(const QString& id) {
    const QString js = QStringLiteral(
        "var el = document.getElementById(%1);"
        "if (el) el.scrollIntoView({behavior:'smooth', block:'start'});"
    ).arg(jsString(id));
    page()->runJavaScript(js);
}
