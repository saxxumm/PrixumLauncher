// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "InstanceDelegate.h"
#include <QApplication>
#include <QDebug>
#include <QPainter>
#include <QTextLayout>
#include <QTextOption>
#include <QtMath>

#include <QIcon>
#include <QTextEdit>
#include "BaseInstance.h"
#include "InstanceList.h"
#include "InstanceView.h"
#include "InstanceWallpaper.h"
#include "ui/themes/NovaTheme.h"

// Origin: Qt
static void viewItemTextLayout(QTextLayout& textLayout, int lineWidth, qreal& height, qreal& widthUsed)
{
    height = 0;
    widthUsed = 0;
    textLayout.beginLayout();
    QString str = textLayout.text();
    while (true) {
        QTextLine line = textLayout.createLine();
        if (!line.isValid())
            break;
        if (line.textLength() == 0)
            break;
        line.setLineWidth(lineWidth);
        line.setPosition(QPointF(0, height));
        height += line.height();
        widthUsed = qMax(widthUsed, line.naturalTextWidth());
    }
    textLayout.endLayout();
}

namespace {
constexpr int s_padding = 8;
constexpr int s_iconTextGap = 6;
}  // namespace

ListViewDelegate::ListViewDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void ListViewDelegate::setMetrics(int itemWidth, int iconSize)
{
    m_itemWidth = itemWidth;
    m_iconSize = iconSize;
}

QRect ListViewDelegate::iconRect(const QRect& card) const
{
    return QRect(card.center().x() - m_iconSize / 2 + 1, card.top() + s_padding + 2, m_iconSize, m_iconSize);
}

QRect ListViewDelegate::textRect(const QRect& card) const
{
    return QRect(card.left() + 4, card.top() + s_padding + 2 + m_iconSize + s_iconTextGap, card.width() - 8,
                 card.height() - (s_padding + 2 + m_iconSize + s_iconTextGap) - s_padding + 2);
}

static void drawBadge(QPainter* painter, const QRect& icon, const QColor& color, const QColor& ring)
{
    const int size = std::max(10, icon.width() / 4);
    const QRectF dot(icon.right() - size + 3, icon.top() - 2, size, size);
    painter->setPen(QPen(ring, 2.5));
    painter->setBrush(color);
    painter->drawEllipse(dot);
}

static QSize viewItemTextSize(const QStyleOptionViewItem* option, int width)
{
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    QTextLayout textLayout;
    textLayout.setTextOption(textOption);
    textLayout.setFont(option->font);
    textLayout.setText(option->text);
    qreal height = 0, widthUsed = 0;
    viewItemTextLayout(textLayout, width, height, widthUsed);
    return QSize(qCeil(widthUsed), qCeil(height));
}

void ListViewDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    painter->save();
    painter->setClipRect(opt.rect);
    painter->setRenderHint(QPainter::Antialiasing, true);

    opt.text = index.data().toString();
    const auto tokens = Nova::current();
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hovered = opt.state & QStyle::State_MouseOver;
    const QRect card = opt.rect.adjusted(1, 1, -1, -1);
    const qreal radius = std::min(tokens.metric("radius"), 16);

    // card background, frosted glass over a wallpaper
    const auto* view = qobject_cast<const InstanceView*>(opt.widget);
    if (const auto* wallpaper = view ? view->wallpaper() : nullptr) {
        wallpaper->paintTile(painter, card, radius, selected, hovered);
    } else if (selected) {
        painter->setPen(QPen(tokens.color("accent"), 1.5));
        painter->setBrush(tokens.color("accentSoft"));
        painter->drawRoundedRect(QRectF(card).adjusted(0.75, 0.75, -0.75, -0.75), radius, radius);
    } else if (hovered) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(tokens.color("hover"));
        painter->drawRoundedRect(card, radius, radius);
    }

    // icon mode and state, also used for badges
    QIcon::Mode mode = QIcon::Normal;
    if (!(opt.state & QStyle::State_Enabled))
        mode = QIcon::Disabled;
    QIcon::State state = opt.state & QStyle::State_Open ? QIcon::On : QIcon::Off;

    const QRect icon = iconRect(card);
    opt.icon.paint(painter, icon, Qt::AlignCenter, mode, state);

    // FIXME: this really has no business of being here. Make generic.
    auto instance = (BaseInstance*)index.data(InstanceList::InstancePointerRole).value<void*>();
    if (instance) {
        const QColor ring = selected ? tokens.color("surface") : (hovered ? tokens.color("hover") : tokens.color("surface"));
        if (instance->isRunning()) {
            drawBadge(painter, icon, tokens.color("success"), ring);
        } else if (instance->hasCrashed() || instance->hasVersionBroken()) {
            drawBadge(painter, icon, tokens.color("danger"), ring);
        }
    }

    // text
    painter->setPen((opt.state & QStyle::State_Enabled) ? tokens.color("text") : tokens.color("textDisabled"));
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    textOption.setTextDirection(opt.direction);
    textOption.setAlignment(QStyle::visualAlignment(opt.direction, Qt::AlignTop | Qt::AlignHCenter));
    QTextLayout textLayout;
    textLayout.setTextOption(textOption);
    textLayout.setFont(opt.font);
    textLayout.setText(opt.text);

    const QRect text = textRect(card);
    qreal width, height;
    viewItemTextLayout(textLayout, text.width(), height, width);
    for (int i = 0; i < textLayout.lineCount(); ++i) {
        textLayout.lineAt(i).draw(painter, text.topLeft());
    }

    painter->restore();
}

QSize ListViewDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.text = index.data().toString();
    const QSize text = viewItemTextSize(&opt, m_itemWidth - 10);
    return QSize(m_itemWidth, s_padding + 2 + m_iconSize + s_iconTextGap + text.height() + s_padding + 2);
}

class NoReturnTextEdit : public QTextEdit {
    Q_OBJECT
   public:
    explicit NoReturnTextEdit(QWidget* parent) : QTextEdit(parent)
    {
        setTextInteractionFlags(Qt::TextEditorInteraction);
        setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    }
    bool event(QEvent* event) override
    {
        auto eventType = event->type();
        if (eventType == QEvent::KeyPress || eventType == QEvent::KeyRelease) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            auto key = keyEvent->key();
            if ((key == Qt::Key_Return || key == Qt::Key_Enter) && eventType == QEvent::KeyPress) {
                emit editingDone();
                return true;
            }
            if (key == Qt::Key_Tab) {
                return true;
            }
        }
        return QTextEdit::event(event);
    }
   signals:
    void editingDone();
};

void ListViewDelegate::updateEditorGeometry(QWidget* editor,
                                            const QStyleOptionViewItem& option,
                                            [[maybe_unused]] const QModelIndex& index) const
{
    editor->setGeometry(textRect(option.rect.adjusted(1, 1, -1, -1)).adjusted(-2, -2, 2, 2));
}

void ListViewDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    auto text = index.data(Qt::EditRole).toString();
    QTextEdit* realEditor = qobject_cast<NoReturnTextEdit*>(editor);
    realEditor->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    realEditor->append(text);
    realEditor->selectAll();
    realEditor->document()->clearUndoRedoStacks();
}

void ListViewDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    QTextEdit* realEditor = qobject_cast<NoReturnTextEdit*>(editor);
    QString text = realEditor->toPlainText();
    text.replace(QChar('\n'), QChar(' '));
    text = text.trimmed();
    // Prevent instance names longer than 128 chars
    text.truncate(128);
    if (text.size() != 0) {
        const auto before = model->data(index).toString();
        model->setData(index, text);
        emit textChanged(before, text);
    }
}

QWidget* ListViewDelegate::createEditor(QWidget* parent,
                                        [[maybe_unused]] const QStyleOptionViewItem& option,
                                        [[maybe_unused]] const QModelIndex& index) const
{
    auto editor = new NoReturnTextEdit(parent);
    connect(editor, &NoReturnTextEdit::editingDone, this, &ListViewDelegate::editingDone);
    return editor;
}

void ListViewDelegate::editingDone()
{
    NoReturnTextEdit* editor = qobject_cast<NoReturnTextEdit*>(sender());
    emit commitData(editor);
    emit closeEditor(editor);
}

#include "InstanceDelegate.moc"
