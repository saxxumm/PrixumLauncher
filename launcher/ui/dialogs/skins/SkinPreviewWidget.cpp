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

#include "SkinPreviewWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <algorithm>

SkinPreviewWidget::SkinPreviewWidget(QWidget* parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::OpenHandCursor);
    m_timer.setInterval(40);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        m_pose.idle = m_clock.elapsed() / 900.0;
        update();
    });
    m_clock.start();
}

void SkinPreviewWidget::setSkin(const QImage& skin, bool slim)
{
    m_skin = skin;
    m_slim = slim;
    update();
}

void SkinPreviewWidget::setCape(const QImage& cape)
{
    m_cape = cape;
    update();
}

void SkinPreviewWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    // a soft shadow on the floor
    const QRectF area = rect().adjusted(16, 12, -16, -12);
    const double floorY = area.center().y() + std::min(area.height() / 37.0, area.width() / 24.0) * 16.5;
    QRadialGradient shadow(QPointF(area.center().x(), floorY), area.width() * 0.32);
    shadow.setColorAt(0, QColor(0, 0, 0, 90));
    shadow.setColorAt(1, QColor(0, 0, 0, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(shadow);
    painter.drawEllipse(QPointF(area.center().x(), floorY), area.width() * 0.32, area.width() * 0.07);
    SkinRenderer::paint(&painter, area, m_skin, m_slim, m_pose, m_cape);
}

void SkinPreviewWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragX = event->position().toPoint().x();
        setCursor(Qt::ClosedHandCursor);
    }
}

void SkinPreviewWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_dragging) {
        return;
    }
    const int x = event->position().toPoint().x();
    m_pose.yaw += (x - m_dragX) * 0.7;
    m_dragX = x;
    update();
}

void SkinPreviewWidget::mouseReleaseEvent(QMouseEvent*)
{
    m_dragging = false;
    setCursor(Qt::OpenHandCursor);
}

void SkinPreviewWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    m_timer.start();
}

void SkinPreviewWidget::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    m_timer.stop();
}
