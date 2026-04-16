#pragma once

#include "../render/MarkdownRenderer.hpp"

#include <QDockWidget>
#include <QString>
#include <vector>

class QTreeWidget;

// The right-hand Table-of-Contents panel. A QDockWidget that shows a tree
// of the current document's headings; clicking one emits `navigateTo` with
// the heading id for the main window to scroll to.
class CTocPanel : public QDockWidget {
    Q_OBJECT

  public:
    explicit CTocPanel(QWidget* parent = nullptr);
    ~CTocPanel() override = default;

    void rebuild(const std::vector<SHeading>& headings);
    void clear();

  signals:
    void navigateTo(const QString& anchorId);

  private:
    QTreeWidget* m_pTree = nullptr;
};
