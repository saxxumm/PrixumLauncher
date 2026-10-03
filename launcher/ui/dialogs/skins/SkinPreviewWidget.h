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

#include <QElapsedTimer>
#include <QImage>
#include <QTimer>
#include <QWidget>

#include "SkinRenderer.h"

/// the big player of the skin manager: turns with the mouse and sways a little while it stands
class SkinPreviewWidget : public QWidget {
    Q_OBJECT

   public:
    explicit SkinPreviewWidget(QWidget* parent = nullptr);

    void setSkin(const QImage& skin, bool slim);
    void setCape(const QImage& cape);

    QSize sizeHint() const override { return { 300, 420 }; }

   protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

   private:
    QImage m_skin;
    QImage m_cape;
    bool m_slim = false;
    SkinRenderer::Pose m_pose;
    QTimer m_timer;
    QElapsedTimer m_clock;
    int m_dragX = 0;
    bool m_dragging = false;
};
