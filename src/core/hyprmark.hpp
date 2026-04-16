#pragma once

#include "../defines.hpp"

#include <memory>
#include <string>
#include <vector>

class QApplication;
class CMainWindow;

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

  private:
    std::unique_ptr<QApplication>          m_pApp;
    std::vector<CMainWindow*>              m_windows; // non-owning; Qt deletes on close
};

inline UP<CHyprmark> g_pHyprmark;
