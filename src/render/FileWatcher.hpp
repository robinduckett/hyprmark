#pragma once

#include "../defines.hpp"

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

// CFileWatcher wraps QFileSystemWatcher to cope with editors that write via
// atomic-rename (vim, etc.). The core trick is to watch the *parent directory*
// rather than the file itself: directory change events fire even when the
// inode is replaced, at which point we re-add the file path.
//
// Two-level debounce:
//   - rapid events collapse into one `changed()` emission
//   - default debounce interval is 100ms
class CFileWatcher : public QObject {
    Q_OBJECT

  public:
    explicit CFileWatcher(QObject* parent = nullptr);
    ~CFileWatcher() override = default;

    // Watch `path`. Pass an empty string to stop watching.
    void watch(const QString& path);

    // Mute for `ms` milliseconds — events during this window are silently
    // dropped. Used so CConfigManager can write to its own config file
    // without triggering a spurious reload of itself.
    void muteFor(int ms);

    void setDebounceMs(int ms);

  signals:
    // Emitted (debounced) when the watched file has been modified on disk.
    // Emitted once per quiescent change — never more than every `debounceMs`.
    void changed(const QString& path);

  private slots:
    void onFileChanged(const QString& path);
    void onDirectoryChanged(const QString& dirPath);

  private:
    void armDebounce();

    QFileSystemWatcher m_watcher;
    QString            m_watchedFile;    // absolute path to the watched file
    QString            m_watchedDir;     // parent directory
    QTimer             m_debounce;
    QTimer             m_muteTimer;
    bool               m_muted      = false;
    int                m_debounceMs = 100;
};
