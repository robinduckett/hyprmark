#pragma once

#include <QWidget>

class QToolButton;
class CHamburgerMenu;

// Thin strip at the top of the central widget. On Hyprland users typically
// disable native decorations anyway; this strip hosts our "≡" hamburger and
// nothing else.
class CTitleBar : public QWidget {
    Q_OBJECT

  public:
    explicit CTitleBar(QWidget* parent = nullptr);
    ~CTitleBar() override = default;

    CHamburgerMenu* menu() const {
        return m_pMenu;
    }

  private:
    QToolButton*    m_pBurger = nullptr;
    CHamburgerMenu* m_pMenu   = nullptr;
};
