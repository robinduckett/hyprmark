#include "TocPanel.hpp"

#include <QTreeWidget>
#include <QTreeWidgetItem>

CTocPanel::CTocPanel(QWidget* parent) : QDockWidget(tr("Contents"), parent) {
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);

    m_pTree = new QTreeWidget(this);
    m_pTree->setHeaderHidden(true);
    m_pTree->setIndentation(14);
    m_pTree->setRootIsDecorated(false);
    m_pTree->setFocusPolicy(Qt::ClickFocus);
    setWidget(m_pTree);

    connect(m_pTree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
        if (!item)
            return;
        const QString id = item->data(0, Qt::UserRole).toString();
        if (!id.isEmpty())
            emit navigateTo(id);
    });
}

void CTocPanel::rebuild(const std::vector<SHeading>& headings) {
    m_pTree->clear();
    // Use a stack so deeper levels nest under the most recent parent.
    std::vector<QTreeWidgetItem*> stack; // parallel to level 1..6
    stack.resize(7, nullptr);

    for (const auto& h : headings) {
        auto* item = new QTreeWidgetItem();
        item->setText(0, QString::fromStdString(h.text));
        item->setData(0, Qt::UserRole, QString::fromStdString(h.id));

        QTreeWidgetItem* parent = nullptr;
        for (int l = h.level - 1; l >= 1; --l) {
            if (stack[l]) {
                parent = stack[l];
                break;
            }
        }
        if (parent)
            parent->addChild(item);
        else
            m_pTree->addTopLevelItem(item);

        stack[h.level] = item;
        // deeper slots are invalidated by a shallower heading
        for (int l = h.level + 1; l < 7; ++l)
            stack[l] = nullptr;
    }
    m_pTree->expandAll();
}

void CTocPanel::clear() {
    m_pTree->clear();
}
