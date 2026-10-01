/* Copyright 2013-2021 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ModListView.h"
#include <QDrag>
#include <QHeaderView>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>

ModListView::ModListView(QWidget* parent) : QTreeView(parent)
{
    setAllColumnsShowFocus(true);
    setExpandsOnDoubleClick(false);
    setRootIsDecorated(false);
    setSortingEnabled(true);
    setAlternatingRowColors(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setHeaderHidden(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setDropIndicatorShown(true);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DropOnly);
    viewport()->setAcceptDrops(true);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
}

void ModListView::setModel(QAbstractItemModel* model)
{
    QTreeView::setModel(model);
    auto head = header();
    head->setStretchLastSection(false);
    // HACK: this is true for the checkbox column of mod lists
    auto string = model->headerData(0, head->orientation()).toString();
    if (head->count() < 1) {
        return;
    }
    if (!string.size()) {
        head->setSectionResizeMode(0, QHeaderView::Interactive);
        head->setSectionResizeMode(1, QHeaderView::Stretch);
        for (int i = 2; i < head->count(); i++)
            head->setSectionResizeMode(i, QHeaderView::Interactive);
    } else {
        head->setSectionResizeMode(0, QHeaderView::Stretch);
        for (int i = 1; i < head->count(); i++)
            head->setSectionResizeMode(i, QHeaderView::Interactive);
    }
}

void ModListView::setResizeModes(const QList<QHeaderView::ResizeMode>& modes)
{
    auto head = header();
    for (int i = 0; i < modes.count(); i++) {
        head->setSectionResizeMode(i, modes[i]);
    }
}

void ModListView::setPlaceholder(const QIcon& icon, const QString& title, const QString& text)
{
    m_placeholderIcon = icon;
    m_placeholderTitle = title;
    m_placeholderText = text;
    viewport()->update();
}

void ModListView::paintEvent(QPaintEvent* event)
{
    QTreeView::paintEvent(event);
    if (!model() || model()->rowCount(rootIndex()) > 0 || m_placeholderTitle.isEmpty()) {
        return;
    }

    QPainter painter(viewport());
    const QRect area = viewport()->rect().adjusted(24, 0, -24, 0);
    const int iconSize = 48;
    const int textWidth = qMin(area.width(), 440);

    QFont titleFont = font();
    titleFont.setBold(true);
    if (titleFont.pointSizeF() > 0) {
        titleFont.setPointSizeF(titleFont.pointSizeF() * 1.2);
    }
    const QFontMetrics titleMetrics(titleFont);
    const QRect textBounds =
        fontMetrics().boundingRect(QRect(0, 0, textWidth, 1000), Qt::AlignHCenter | Qt::TextWordWrap, m_placeholderText);

    const int spacing = 12;
    const int total = iconSize + spacing + titleMetrics.height() + 6 + textBounds.height();
    int y = area.center().y() - total / 2;

    m_placeholderIcon.paint(&painter, QRect(area.center().x() - iconSize / 2, y, iconSize, iconSize));
    y += iconSize + spacing;

    painter.setFont(titleFont);
    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(QRect(area.left(), y, area.width(), titleMetrics.height()), Qt::AlignCenter, m_placeholderTitle);
    y += titleMetrics.height() + 6;

    painter.setFont(font());
    painter.setPen(palette().color(QPalette::PlaceholderText));
    painter.drawText(QRect(area.center().x() - textWidth / 2, y, textWidth, textBounds.height()), Qt::AlignHCenter | Qt::TextWordWrap,
                     m_placeholderText);
}
