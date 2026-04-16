#pragma once

#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>

#include <atomic>

// CUrlInterceptor enforces hyprmark's URL scheme policy (spec §5.9):
//   - data:, file:, qrc:  -> allow
//   - http:, https:       -> allow only if m_allowRemote, else block + count
//   - anything else       -> block
//
// Blocked http(s) image requests are counted per-render and surfaced via the
// remoteBlocked signal so the main window can offer the opt-in prompt.
class CUrlInterceptor : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT

  public:
    explicit CUrlInterceptor(QObject* parent = nullptr);
    ~CUrlInterceptor() override = default;

    void interceptRequest(QWebEngineUrlRequestInfo& info) override;

    void setAllowRemote(bool allow);
    bool allowRemote() const;

    // Reset the blocked counter before a new render. Call from the main thread.
    void resetBlockedCount();
    int  blockedCount() const;

  signals:
    // Emitted on the interceptor thread when we hit our first blocked remote
    // request for the current render. Connect queued to marshal back to main.
    void remoteBlocked();

  private:
    std::atomic<bool> m_allowRemote{false};
    std::atomic<int>  m_blocked{0};
};
