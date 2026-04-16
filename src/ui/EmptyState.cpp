#include "EmptyState.hpp"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLabel>
#include <QMimeData>
#include <QPushButton>
#include <QVBoxLayout>

CEmptyState::CEmptyState(QWidget* parent) : QWidget(parent) {
    setAcceptDrops(true);
    setAutoFillBackground(true);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(24, 24, 24, 24);

    // inner frame — a visible dashed drop target
    auto* frame = new QWidget(this);
    frame->setObjectName("hmDropZone");
    frame->setStyleSheet(
        "QWidget#hmDropZone {"
        "  border: 2px dashed rgba(180,120,255,0.35);"
        "  border-radius: 12px;"
        "  background: rgba(180,120,255,0.04);"
        "}"
        "QWidget#hmDropZoneHover {"
        "  border: 2px solid rgba(180,120,255,0.9);"
        "  border-radius: 12px;"
        "  background: rgba(180,120,255,0.12);"
        "}"
    );
    outerLayout->addWidget(frame);

    auto* layout = new QVBoxLayout(frame);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(16);

    auto* arrow = new QLabel(QStringLiteral("↓"));
    arrow->setAlignment(Qt::AlignCenter);
    arrow->setStyleSheet("font-size: 36px; color: #b478ff;");
    layout->addWidget(arrow);

    auto* title = new QLabel(QStringLiteral("Drop a <code>.md</code> file here"));
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 14px;");
    title->setTextFormat(Qt::RichText);
    layout->addWidget(title);

    m_pChooseBtn = new QPushButton(QStringLiteral("or choose a file…"));
    m_pChooseBtn->setFocusPolicy(Qt::StrongFocus);
    m_pChooseBtn->setStyleSheet(
        "QPushButton {"
        "  padding: 8px 18px; font-size: 13px; border-radius: 8px;"
        "  border: 1px solid rgba(180,120,255,0.35);"
        "  background: rgba(180,120,255,0.10);"
        "}"
        "QPushButton:hover { background: rgba(180,120,255,0.20); }"
        "QPushButton:pressed { background: rgba(180,120,255,0.30); }"
    );
    connect(m_pChooseBtn, &QPushButton::clicked, this, &CEmptyState::openRequested);
    layout->addWidget(m_pChooseBtn, 0, Qt::AlignCenter);

    auto* hint = new QLabel(QStringLiteral("Ctrl+O"));
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("color: rgba(168,156,200,0.65); font-size: 11px;");
    layout->addWidget(hint);
}

void CEmptyState::dragEnterEvent(QDragEnterEvent* event) {
    // Accept anything with URLs or markdown text; MainWindow's dropEvent does
    // the real filtering.
    if (event->mimeData()->hasUrls() || event->mimeData()->hasFormat(QStringLiteral("text/markdown"))) {
        event->acceptProposedAction();
        setDropHover(true);
    }
}

void CEmptyState::dragLeaveEvent(QDragLeaveEvent* event) {
    (void)event;
    setDropHover(false);
}

void CEmptyState::dropEvent(QDropEvent* event) {
    (void)event;
    setDropHover(false);
    // The actual file-handling drop lives on CMainWindow so we don't duplicate
    // the validation logic here; letting this event bubble would be ideal but
    // Qt stops propagating once we accept in dragEnterEvent. Re-emit as a drop
    // on the parent via its public handler — simplest is to let CMainWindow's
    // dropEvent also run. We do NOT accept here so the event bubbles up.
    event->ignore();
}

void CEmptyState::setDropHover(bool on) {
    if (m_dropHover == on)
        return;
    m_dropHover = on;
    auto* frame = findChild<QWidget*>(QStringLiteral("hmDropZone"));
    if (!frame) return;
    frame->setObjectName(on ? QStringLiteral("hmDropZoneHover") : QStringLiteral("hmDropZone"));
    // force re-style
    frame->setStyleSheet(frame->styleSheet());
}
