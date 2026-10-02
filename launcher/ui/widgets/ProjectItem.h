#pragma once

#include <QStyledItemDelegate>

/* Custom data types for our custom list models :) */
enum UserDataTypes {
    TITLE = 257,        // QString
    DESCRIPTION = 258,  // QString
    INSTALLED = 259,    // bool
    AUTHOR = 260,       // QString
    DOWNLOADS = 261     // qint64, missing when unknown
};

/** This is an item delegate composed of:
 *  - An Icon on the left
 *  - A title with the authors and the download count next to it
 *  - A description of up to two lines
 *  - A button to pick the item on the right, for lists with check states
 * */
class ProjectItemDelegate final : public QStyledItemDelegate {
    Q_OBJECT

   public:
    ProjectItemDelegate(QWidget* parent);

    void paint(QPainter*, const QStyleOptionViewItem&, const QModelIndex&) const override;

    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;

    /// "↓ 12.3M", the download count the way the cards show it
    static QString downloadsText(qint64 downloads);

   signals:
    void checkboxClicked(const QModelIndex& index);

   private:
    /// the pick button of an item, empty for lists without check states
    QRect toggleRect(const QStyleOptionViewItem& opt, const QModelIndex& index) const;
    QString toggleText(const QModelIndex& index, Qt::CheckState state) const;
};
