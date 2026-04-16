#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;
class QToolButton;

// Thin horizontal strip shown at the top of the central area when the user
// presses Ctrl+F. Uses QWebEnginePage::findText for live, case-insensitive,
// multi-match search. Emits signals the main window forwards to the active
// CRenderView — keeps this widget decoupled from QtWebEngine headers.
class CFindBar : public QWidget {
    Q_OBJECT

  public:
    explicit CFindBar(QWidget* parent = nullptr);
    ~CFindBar() override = default;

    // Show the bar, clear existing state, and focus the input. Called from
    // CMainWindow's Ctrl+F shortcut.
    void activate();

    // Hide the bar and emit findCleared() so the host clears highlights.
    void dismiss();

    // Update the "N of M" indicator after findText returns.
    void setResult(int active, int total);

  signals:
    // Emitted as the user types — issue a forward, case-insensitive search.
    void findRequested(const QString& query);
    // Enter / Shift+Enter / Next / Prev buttons.
    void findNext();
    void findPrevious();
    // Escape or close button — clear the current highlight.
    void findCleared();

  protected:
    void keyPressEvent(QKeyEvent* event) override;

  private:
    QLineEdit*   m_pInput   = nullptr;
    QLabel*      m_pResult  = nullptr;
    QToolButton* m_pPrev    = nullptr;
    QToolButton* m_pNext    = nullptr;
    QToolButton* m_pClose   = nullptr;
};
