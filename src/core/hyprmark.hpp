#pragma once

#include "../defines.hpp"

#include <memory>
#include <string>
#include <vector>

class QApplication;
class CMainWindow;
class CFileOpenRouter;

class CHyprmark {
  public:
    CHyprmark();
    ~CHyprmark();

    // main event loop entrypoint. non-dispatch args have already been parsed.
    int run(int argc, char** argv, const std::string& configPath, const std::string& filePath);

    // Open a new top-level window. Empty path => empty-state. Returns the
    // window (owned by the Qt parent tree via WA_DeleteOnClose).
    CMainWindow* newWindow(const std::string& filePath);

    // The currently active / focused main window, or the last-created one
    // as a fallback. Null if no windows exist.
    CMainWindow* activeWindow() const;

    const std::vector<CMainWindow*>& windows() const { return m_windows; }

    // Entry point for documents handed to us by the platform rather than argv
    // (macOS Finder "Open With", `open -a hyprmark foo.md`, dock drops; see
    // CFileOpenRouter). Loads into a window that is still showing the empty
    // state if there is one, otherwise spawns a new window.
    void openFromSystem(const std::string& filePath);

  private:
    // Deterministic teardown, reverse of construction: windows (every
    // QWebEngineView/Page), IPC server, config manager, then the
    // QApplication itself. Must run while main() is still on the stack;
    // see the comment at the end of run().
    void shutdown();

    std::unique_ptr<QApplication>          m_pApp;
    CFileOpenRouter*                       m_pFileOpenRouter = nullptr; // owned by m_pApp
    std::vector<CMainWindow*>              m_windows; // non-owning; Qt deletes on close
};

inline UP<CHyprmark> g_pHyprmark;
