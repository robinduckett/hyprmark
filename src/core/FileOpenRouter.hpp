#pragma once

#include <QObject>

#include <functional>
#include <string>

// CFileOpenRouter turns platform "open this document" requests into a plain
// callback.
//
// macOS never passes a document on argv: Finder's "Open With", double-clicks
// on an associated file, `open -a hyprmark foo.md` and dock drops all arrive
// as a file-open Apple Event, which Qt surfaces as a QFileOpenEvent sent to
// the QApplication object. If the app is already running, Launch Services
// re-uses that process instead of starting a second one, so this is also the
// only way such documents reach a running instance. Without a handler the
// app just comes up empty.
//
// Install with qApp->installEventFilter(router). Harmless on platforms that
// never emit QFileOpenEvent.
class CFileOpenRouter : public QObject {
    Q_OBJECT

  public:
    using Handler = std::function<void(const std::string& path)>;

    explicit CFileOpenRouter(Handler handler, QObject* parent = nullptr);

    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    Handler m_handler;
};
