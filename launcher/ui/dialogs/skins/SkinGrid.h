// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QAbstractListModel>
#include <QStyledItemDelegate>

class SkinList;

/// the saved skins with an "add skin" card in front
class SkinGridModel : public QAbstractListModel {
    Q_OBJECT

   public:
    enum Roles { AddCardRole = Qt::UserRole + 100, TextureRole, SlimRole, WornRole };

    explicit SkinGridModel(SkinList* skins, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    QStringList mimeTypes() const override;
    Qt::DropActions supportedDropActions() const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;

    /// the card of a row of the skin list
    QModelIndex cardOf(int skinRow) const { return skinRow < 0 ? QModelIndex() : index(skinRow + 1); }

   private:
    QModelIndex skinIndex(const QModelIndex& card) const;

    SkinList* m_skins;
};

/// draws every skin as a card with the player walking, the time comes from the view's animation timer
class SkinCardDelegate : public QStyledItemDelegate {
    Q_OBJECT

   public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void setTime(double seconds) { m_seconds = seconds; }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

   private:
    double m_seconds = 0;
};
