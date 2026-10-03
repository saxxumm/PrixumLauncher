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

#include "SkinGrid.h"

#include <QApplication>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include "SkinRenderer.h"
#include "minecraft/skins/SkinList.h"

SkinGridModel::SkinGridModel(SkinList* skins, QObject* parent) : QAbstractListModel(parent), m_skins(skins)
{
    // every row of the list is one row further down here
    connect(skins, &QAbstractItemModel::modelAboutToBeReset, this, &SkinGridModel::beginResetModel);
    connect(skins, &QAbstractItemModel::modelReset, this, &SkinGridModel::endResetModel);
    connect(skins, &QAbstractItemModel::rowsAboutToBeInserted, this,
            [this](const QModelIndex&, int first, int last) { beginInsertRows({}, first + 1, last + 1); });
    connect(skins, &QAbstractItemModel::rowsInserted, this, &SkinGridModel::endInsertRows);
    connect(skins, &QAbstractItemModel::rowsAboutToBeRemoved, this,
            [this](const QModelIndex&, int first, int last) { beginRemoveRows({}, first + 1, last + 1); });
    connect(skins, &QAbstractItemModel::rowsRemoved, this, &SkinGridModel::endRemoveRows);
    connect(skins, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex& topLeft, const QModelIndex& bottomRight) {
        emit dataChanged(cardOf(topLeft.row()), cardOf(bottomRight.row()));
    });
}

QModelIndex SkinGridModel::skinIndex(const QModelIndex& card) const
{
    return card.isValid() && card.row() > 0 ? m_skins->index(card.row() - 1) : QModelIndex();
}

int SkinGridModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_skins->rowCount() + 1;
}

QVariant SkinGridModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return {};
    }
    if (index.row() == 0) {
        return role == AddCardRole ? QVariant(true) : QVariant();
    }
    const QModelIndex source = skinIndex(index);
    switch (role) {
        case AddCardRole:
            return false;
        case TextureRole:
        case SlimRole: {
            const auto* skin = m_skins->skin(source.data(Qt::UserRole).toString());
            if (!skin) {
                return {};
            }
            return role == TextureRole ? QVariant(skin->getTexture()) : QVariant(skin->getModel() == SkinModel::SLIM);
        }
        case WornRole:
            return source.row() == m_skins->getSelectedAccountSkin();
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case Qt::UserRole:
        case Qt::EditRole:
            return source.data(role);
        default:
            return {};
    }
}

bool SkinGridModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    return m_skins->setData(skinIndex(index), value, role);
}

Qt::ItemFlags SkinGridModel::flags(const QModelIndex& index) const
{
    if (index.isValid() && index.row() == 0) {
        return Qt::ItemIsEnabled | Qt::ItemIsDropEnabled;
    }
    return m_skins->flags(skinIndex(index));
}

QStringList SkinGridModel::mimeTypes() const
{
    return m_skins->mimeTypes();
}

Qt::DropActions SkinGridModel::supportedDropActions() const
{
    return m_skins->supportedDropActions();
}

bool SkinGridModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
    return m_skins->dropMimeData(data, action, row, column, skinIndex(parent));
}

void SkinCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    // the grid itself is transparent, the cards take the colors of the application
    const QPalette palette = QApplication::palette();
    const QRectF card = QRectF(option.rect).adjusted(6, 6, -6, -6);
    const bool hovered = option.state & QStyle::State_MouseOver;
    const bool selected = option.state & QStyle::State_Selected;

    if (index.data(SkinGridModel::AddCardRole).toBool()) {
        painter->setBrush(hovered ? palette.color(QPalette::Midlight) : Qt::transparent);
        QPen pen(palette.color(QPalette::Dark), 1.5, Qt::DashLine);
        painter->setPen(pen);
        painter->drawRoundedRect(card.adjusted(0.75, 0.75, -0.75, -0.75), 14, 14);
        const QPointF center(card.center().x(), card.center().y() - 26);
        QPen plus(palette.color(QPalette::Text), 2.5);
        plus.setCapStyle(Qt::RoundCap);
        painter->setPen(plus);
        painter->drawLine(center - QPointF(13, 0), center + QPointF(13, 0));
        painter->drawLine(center - QPointF(0, 13), center + QPointF(0, 13));
        QFont title = option.font;
        title.setBold(true);
        painter->setFont(title);
        painter->drawText(QRectF(card.left() + 8, center.y() + 26, card.width() - 16, 24), Qt::AlignCenter, tr("Add skin"));
        painter->setFont(option.font);
        painter->setPen(palette.color(QPalette::PlaceholderText));
        painter->drawText(QRectF(card.left() + 10, center.y() + 52, card.width() - 20, 44),
                          Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, tr("Drop a file or a link here"));
        painter->restore();
        return;
    }

    painter->setBrush(hovered && !selected ? palette.color(QPalette::Midlight) : palette.color(QPalette::Base));
    painter->setPen(selected ? QPen(palette.color(QPalette::Highlight), 2) : QPen(palette.color(QPalette::Mid), 1));
    painter->drawRoundedRect(card.adjusted(1, 1, -1, -1), 14, 14);

    // every card a little out of step with its neighbours
    SkinRenderer::Pose pose;
    pose.walk = m_seconds * 4.2 + index.row() * 0.9;
    pose.stride = 1;
    const QImage texture = index.data(SkinGridModel::TextureRole).value<QImage>();
    const QRectF model = card.adjusted(10, 12, -10, -34);
    SkinRenderer::paint(painter, model, texture, index.data(SkinGridModel::SlimRole).toBool(), pose);

    const QRectF nameRect(card.left() + 10, card.bottom() - 30, card.width() - 20, 22);
    painter->setPen(palette.color(QPalette::PlaceholderText));
    painter->setFont(option.font);
    painter->drawText(
        nameRect, Qt::AlignCenter,
        option.fontMetrics.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideMiddle, static_cast<int>(nameRect.width())));

    if (index.data(SkinGridModel::WornRole).toBool()) {
        QFont badge = option.font;
        badge.setBold(true);
        badge.setPointSizeF(std::max(7.0, badge.pointSizeF() - 1));
        const QFontMetrics metrics(badge);
        const QString text = tr("Worn");
        const QRectF pill(card.left() + 10, card.top() + 10, metrics.horizontalAdvance(text) + 18, metrics.height() + 6);
        painter->setPen(Qt::NoPen);
        painter->setBrush(palette.color(QPalette::Highlight));
        painter->drawRoundedRect(pill, pill.height() / 2, pill.height() / 2);
        painter->setPen(palette.color(QPalette::HighlightedText));
        painter->setFont(badge);
        painter->drawText(pill, Qt::AlignCenter, text);
    }
    painter->restore();
}

QSize SkinCardDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const
{
    return { 168, 228 };
}

void SkinCardDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex&) const
{
    // renaming happens where the name is
    const QRect card = option.rect.adjusted(6, 6, -6, -6);
    editor->setGeometry(card.left() + 8, card.bottom() - 36, card.width() - 16, 30);
}
