#include "FindBar.hpp"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

CFindBar::CFindBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(32);
    setAutoFillBackground(true);
    setStyleSheet(
        "CFindBar { background: rgba(180,120,255,0.08); border-bottom: 1px solid rgba(180,120,255,0.20); }"
        "QLineEdit { padding: 3px 8px; border: 1px solid rgba(180,120,255,0.35); border-radius: 4px; background: transparent; }"
        "QLineEdit:focus { border-color: rgba(180,120,255,0.75); }"
        "QToolButton { padding: 2px 8px; font-size: 14px; border: 1px solid transparent; border-radius: 4px; }"
        "QToolButton:hover { background: rgba(180,120,255,0.18); }"
        "QLabel#findResult { color: rgba(150,140,180,1); font-size: 11px; }"
    );

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(6);

    m_pInput = new QLineEdit(this);
    m_pInput->setPlaceholderText(tr("Find…"));
    m_pInput->setClearButtonEnabled(true);
    layout->addWidget(m_pInput, 1);

    m_pResult = new QLabel(QStringLiteral(""), this);
    m_pResult->setObjectName(QStringLiteral("findResult"));
    m_pResult->setMinimumWidth(60);
    m_pResult->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(m_pResult);

    m_pPrev = new QToolButton(this);
    m_pPrev->setText(QStringLiteral("▲"));
    m_pPrev->setToolTip(tr("Previous match (Shift+Enter)"));
    layout->addWidget(m_pPrev);

    m_pNext = new QToolButton(this);
    m_pNext->setText(QStringLiteral("▼"));
    m_pNext->setToolTip(tr("Next match (Enter)"));
    layout->addWidget(m_pNext);

    m_pClose = new QToolButton(this);
    m_pClose->setText(QStringLiteral("×"));
    m_pClose->setToolTip(tr("Close (Esc)"));
    layout->addWidget(m_pClose);

    connect(m_pInput, &QLineEdit::textChanged, this, [this](const QString& t) {
        emit findRequested(t);
        if (t.isEmpty())
            setResult(0, 0);
    });
    connect(m_pInput, &QLineEdit::returnPressed, this, &CFindBar::findNext);
    connect(m_pNext,  &QToolButton::clicked,   this, &CFindBar::findNext);
    connect(m_pPrev,  &QToolButton::clicked,   this, &CFindBar::findPrevious);
    connect(m_pClose, &QToolButton::clicked,   this, &CFindBar::dismiss);

    hide();
}

void CFindBar::activate() {
    setResult(0, 0);
    show();
    m_pInput->selectAll();
    m_pInput->setFocus(Qt::ShortcutFocusReason);
}

void CFindBar::dismiss() {
    hide();
    m_pInput->clear();
    setResult(0, 0);
    emit findCleared();
    if (auto* p = parentWidget())
        p->setFocus();
}

void CFindBar::setResult(int active, int total) {
    if (total <= 0)
        m_pResult->setText(QString());
    else
        m_pResult->setText(QStringLiteral("%1 / %2").arg(active).arg(total));
}

void CFindBar::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        dismiss();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (event->modifiers() & Qt::ShiftModifier)
            emit findPrevious();
        else
            emit findNext();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}
