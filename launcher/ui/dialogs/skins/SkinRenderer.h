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

#include <QImage>
#include <QRectF>

class QPainter;

/**
 * Draws a Minecraft player with QPainter: every face of the model is a parallelogram under an orthographic camera,
 * so it is an affine transform of its texture rectangle. Needs no OpenGL and keeps the pixels sharp.
 */
namespace SkinRenderer {

struct Pose {
    /// degrees around the vertical axis, negative turns the player's right side towards the viewer
    double yaw = -28;
    /// degrees, positive looks from above
    double pitch = 12;
    /// walk cycle in radians, one step per pi
    double walk = 0;
    /// 0 stands still, 1 is a full stride
    double stride = 0;
    /// idle sway in radians, for a player that stands
    double idle = 0;
};

/// paints the player as large as fits into rect, centered; skin is a 64 x 64 texture
void paint(QPainter* painter, const QRectF& rect, const QImage& skin, bool slim, const Pose& pose, const QImage& cape = {});

}  // namespace SkinRenderer
