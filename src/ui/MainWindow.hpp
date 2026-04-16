#pragma once

#include <QMainWindow>
#include <QShortcut>
#include <QString>
#include <filesystem>
#include <vector>

class QStackedWidget;
class CRenderView;
class CEmptyState;
class CTitleBar;
class CTocPanel;

class CMainWindow : public QMainWindow {
    Q_OBJECT

  public:
    explicit CMainWindow(QWidget* parent = nullptr);
    ~CMainWindow() override;

    // Load the given file and display it. Empty path => empty-state.
    void openFile(const std::filesystem::path& path);

    // Prompt the user via QFileDialog and open the selection (no-op on cancel).
    void promptForFile();

    // Same, but spawn a fresh window for the result instead of swapping.
    void promptForFileInNewWindow();

    // Spawn an empty-state window immediately (no dialog).
    void newEmptyWindow();

    void applyTheme(const QString& name);
    void cycleTheme();

    void zoomIn();
    void zoomOut();
    void zoomReset();

    void findInPage();
    void toggleToc();       // placeholder, real impl in M7
    void exportPdf();       // placeholder, real impl in M9

    // Status bar helpers.
    void showInfo(const QString& text, int timeoutMs = 3000);
    void showWarn(const QString& text);
    void showError(const QString& text);

    // Persistent status-bar widget for the "remote images blocked" prompt.
    void showRemoteImagesPrompt();
    void hideRemoteImagesPrompt();

    CRenderView* renderView() const {
        return m_pRenderView;
    }

  public slots:
    // Exposed for IPC. Re-renders the current file in place, preserving scroll.
    void onSourceFileChanged();

  private slots:
    void onConfigChanged(const QString& key);
    void onConfigReloaded();

  protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    // Return true if `url` is a file:// URL whose extension we accept.
    static bool isAcceptedMarkdownUrl(const class QUrl& url);

    void wireKeybinds();
    void attachSourceWatcher(const QString& path);
    void detachSourceWatcher();

    QStackedWidget* m_pStack      = nullptr;
    CEmptyState*    m_pEmptyState = nullptr;
    CRenderView*    m_pRenderView = nullptr;
    CTitleBar*      m_pTitleBar   = nullptr;
    CTocPanel*      m_pTocPanel   = nullptr;
    class CFileWatcher* m_pSourceWatcher = nullptr;
    class QWidget*      m_pRemotePrompt  = nullptr;
    std::filesystem::path m_currentFile;
    bool                  m_hasLoadedContent = false;
    QString               m_currentTheme;
    double                m_zoom = 1.0;

    std::vector<QShortcut*> m_shortcuts;
};
