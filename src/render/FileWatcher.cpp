#include "FileWatcher.hpp"

#include "../helpers/Log.hpp"

#include <QFileInfo>

CFileWatcher::CFileWatcher(QObject* parent) : QObject(parent) {
    m_debounce.setSingleShot(true);
    m_muteTimer.setSingleShot(true);

    connect(&m_watcher, &QFileSystemWatcher::fileChanged,      this, &CFileWatcher::onFileChanged);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &CFileWatcher::onDirectoryChanged);
    connect(&m_debounce, &QTimer::timeout, this, [this]() {
        if (!m_watchedFile.isEmpty())
            emit changed(m_watchedFile);
    });
    connect(&m_muteTimer, &QTimer::timeout, this, [this]() { m_muted = false; });
}

void CFileWatcher::watch(const QString& path) {
    // cleanly drop whatever we had before
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
    m_watchedFile.clear();
    m_watchedDir.clear();

    if (path.isEmpty())
        return;

    const QFileInfo info(path);
    if (!info.exists()) {
        Debug::log(WARN, "CFileWatcher::watch({}) — file does not exist", path.toStdString());
        // still watch the directory so we notice it being created
    }

    m_watchedFile = info.absoluteFilePath();
    m_watchedDir  = info.absolutePath();

    if (info.exists())
        m_watcher.addPath(m_watchedFile);
    m_watcher.addPath(m_watchedDir);
}

void CFileWatcher::muteFor(int ms) {
    m_muted = true;
    m_muteTimer.start(ms);
}

void CFileWatcher::setDebounceMs(int ms) {
    m_debounceMs = ms;
}

void CFileWatcher::armDebounce() {
    if (m_muted)
        return;
    m_debounce.start(m_debounceMs);
}

void CFileWatcher::onFileChanged(const QString& path) {
    // Editors that write-rename may briefly remove the inode we were watching;
    // re-add it so we keep getting signals.
    if (path == m_watchedFile && QFileInfo::exists(m_watchedFile) && !m_watcher.files().contains(m_watchedFile))
        m_watcher.addPath(m_watchedFile);
    armDebounce();
}

void CFileWatcher::onDirectoryChanged(const QString& dirPath) {
    (void)dirPath;
    // A rename-over event arrives here; re-add the file path if it exists.
    if (!m_watchedFile.isEmpty() && QFileInfo::exists(m_watchedFile) && !m_watcher.files().contains(m_watchedFile))
        m_watcher.addPath(m_watchedFile);
    armDebounce();
}
