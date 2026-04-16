#pragma once

#include <QMenu>

class QActionGroup;

// The pop-up menu opened by the "≡" toolbutton. Owns the Theme submenu
// and exposes its actions via Qt signals so CMainWindow stays ignorant of
// the exact layout.
class CHamburgerMenu : public QMenu {
    Q_OBJECT

  public:
    explicit CHamburgerMenu(QWidget* parent = nullptr);
    ~CHamburgerMenu() override = default;

    // Rebuild the Theme submenu from the current CThemeManager catalog.
    // Sets a radio-check on `currentTheme`.
    void rebuildThemeMenu(const QString& currentTheme);

    // Toggle visibility of the "Disable remote images" entry.
    void setRemoteImagesVisible(bool visible);

  signals:
    void openRequested();
    void openInNewWindowRequested();
    void newEmptyWindowRequested();
    void themeSelected(const QString& name);
    void toggleTocRequested();
    void findRequested();
    void zoomInRequested();
    void zoomOutRequested();
    void zoomResetRequested();
    void exportPdfRequested();
    void disableRemoteImagesRequested();
    void aboutRequested();
    void quitRequested();

  private:
    QMenu*        m_pThemeMenu         = nullptr;
    QActionGroup* m_pThemeGroup        = nullptr;
    QAction*      m_pDisableRemoteAct  = nullptr;
};
