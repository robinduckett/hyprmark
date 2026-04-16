#include "TitleBar.hpp"

#include "HamburgerMenu.hpp"

#include <QHBoxLayout>
#include <QToolButton>

CTitleBar::CTitleBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(32);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->addStretch(1);

    m_pMenu   = new CHamburgerMenu(this);
    m_pBurger = new QToolButton(this);
    m_pBurger->setText(QStringLiteral("≡"));
    m_pBurger->setCursor(Qt::PointingHandCursor);
    m_pBurger->setPopupMode(QToolButton::InstantPopup);
    m_pBurger->setMenu(m_pMenu);
    m_pBurger->setAutoRaise(true);
    m_pBurger->setStyleSheet(
        "QToolButton { font-size: 18px; padding: 2px 10px; border-radius: 4px; }"
        "QToolButton::menu-indicator { image: none; width: 0; }"
        "QToolButton:hover { background: rgba(180,120,255,0.15); }");
    layout->addWidget(m_pBurger);
}
