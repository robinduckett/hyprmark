#pragma once

#include <QWidget>

class QPushButton;

// CEmptyState is the full-window drop zone shown before any file is loaded.
// Matches the chosen "Full drop-zone" design from brainstorming.
class CEmptyState : public QWidget {
    Q_OBJECT

  public:
    explicit CEmptyState(QWidget* parent = nullptr);
    ~CEmptyState() override = default;

  signals:
    // Emitted when the user clicks the "choose a file…" button.
    void openRequested();

  protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

  private:
    void setDropHover(bool on);

    QPushButton* m_pChooseBtn = nullptr;
    bool         m_dropHover  = false;
};
