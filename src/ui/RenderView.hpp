#pragma once

#include <QUrl>
#include <QWebEnginePage>
#include <QWebEngineView>

class QWebChannel;
class CBridge;
class CUrlInterceptor;

// CRenderView is the rendered-markdown surface: a QWebEngineView wired to a
// CBridge over QWebChannel and a CUrlInterceptor enforcing scheme policy.
class CRenderView : public QWebEngineView {
    Q_OBJECT

  public:
    explicit CRenderView(QWidget* parent = nullptr);
    ~CRenderView() override;

    CBridge*         bridge() const { return m_bridge; }
    CUrlInterceptor* interceptor() const { return m_interceptor; }

    // First render: sets content + baseUrl so relative images resolve against
    // the source document's directory. Triggers a full page load.
    void loadInitial(const QString& fullPageHtml, const QUrl& baseUrl);

    // Subsequent renders (live-reload, drag-drop of a different file): swap
    // the inner body in place via JS so we don't lose scroll position.
    void swapBody(const QString& bodyHtml, const QString& sourcePath);

    // Theme change.
    void swapTheme(const QString& themeUrl);

    void setScale(double factor);

    // Scroll the page to an element with the given id attribute (from the TOC).
    void scrollToAnchor(const QString& id);

    // Find-bar API. `findText` re-issues a case-insensitive forward search
    // each time the query string changes; `findNext`/`findPrevious` step
    // through the cached result set; `findClear` drops all highlights.
    void findText(const QString& query);
    void findNext();
    void findPrevious();
    void findClear();

  signals:
    // Emitted after each findText call with the Qt 6.8 FindTextResult, so the
    // find bar can show "N of M".
    void findResult(int activeMatch, int totalMatches);

  private:
    CBridge*         m_bridge      = nullptr;
    QWebChannel*     m_channel     = nullptr;
    CUrlInterceptor* m_interceptor = nullptr;
    QString          m_lastFindQuery;
};
