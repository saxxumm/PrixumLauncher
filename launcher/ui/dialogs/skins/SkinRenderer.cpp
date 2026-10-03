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

#include "SkinRenderer.h"

#include <QList>
#include <QMatrix4x4>
#include <QPainter>
#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace SkinRenderer {

namespace {

struct Face {
    QVector3D topLeft;
    QVector3D topRight;
    QVector3D bottomLeft;
    QRectF texture;
    const QImage* image;
    float depth = 0;
};

struct Box {
    QVector3D min;
    QVector3D max;
    /// corner of the box layout in the texture
    QPointF uv;
    /// width, height and depth in texture pixels, before the overlay grows the box
    QVector3D size;
    float grow = 0;
    QMatrix4x4 pose;
    const QImage* image;
    /// texture pixels per model pixel, above 1 for HD capes
    double texelScale = 1;
};

/// the six faces of a box with the texture layout Minecraft uses for every part
void addBox(QList<Face>& faces, const Box& box, const QMatrix4x4& view)
{
    const float g = box.grow;
    const float x0 = box.min.x() - g, y0 = box.min.y() - g, z0 = box.min.z() - g;
    const float x1 = box.max.x() + g, y1 = box.max.y() + g, z1 = box.max.z() + g;
    const double w = box.size.x(), h = box.size.y(), d = box.size.z();
    const double u = box.uv.x(), v = box.uv.y();
    const QMatrix4x4 transform = view * box.pose;
    auto add = [&](QVector3D topLeft, QVector3D topRight, QVector3D bottomLeft, QRectF rect) {
        const double s = box.texelScale;
        faces.append({ transform.map(topLeft), transform.map(topRight), transform.map(bottomLeft),
                       QRectF(rect.x() * s, rect.y() * s, rect.width() * s, rect.height() * s), box.image });
    };
    add({ x0, y1, z1 }, { x1, y1, z1 }, { x0, y0, z1 }, { u + d, v + d, w, h });          // front
    add({ x1, y1, z0 }, { x0, y1, z0 }, { x1, y0, z0 }, { u + d + w + d, v + d, w, h });  // back
    add({ x0, y1, z0 }, { x0, y1, z1 }, { x0, y0, z0 }, { u, v + d, d, h });              // right side of the player
    add({ x1, y1, z1 }, { x1, y1, z0 }, { x1, y0, z1 }, { u + d + w, v + d, d, h });      // left side of the player
    add({ x0, y1, z0 }, { x1, y1, z0 }, { x0, y1, z1 }, { u + d, v, w, d });              // top
    add({ x0, y0, z1 }, { x1, y0, z1 }, { x0, y0, z0 }, { u + d + w, v, w, d });          // bottom
}

QMatrix4x4 rotation(const QVector3D& pivot, float angle, const QVector3D& axis)
{
    QMatrix4x4 m;
    m.translate(pivot);
    m.rotate(angle, axis);
    m.translate(-pivot);
    return m;
}

}  // namespace

void paint(QPainter* painter, const QRectF& rect, const QImage& skin, bool slim, const Pose& pose, const QImage& cape)
{
    if (skin.isNull() || rect.isEmpty()) {
        return;
    }

    const double swing = std::sin(pose.walk) * pose.stride;
    const float legAngle = static_cast<float>(34 * swing);
    const float armAngle = static_cast<float>(-30 * swing);
    // arms drift away from the body a little while standing
    const float armSpread = static_cast<float>(2.5 + 1.5 * std::sin(pose.idle));
    const float bob = static_cast<float>(0.6 * pose.stride * std::abs(std::cos(pose.walk)));

    QMatrix4x4 body;
    body.translate(0, bob, 0);
    const float armWidth = slim ? 3 : 4;
    const QMatrix4x4 rightArm =
        body * rotation({ -4 - armWidth / 2, 22, 0 }, armAngle, { 1, 0, 0 }) * rotation({ -4, 24, 0 }, -armSpread, { 0, 0, 1 });
    const QMatrix4x4 leftArm =
        body * rotation({ 4 + armWidth / 2, 22, 0 }, -armAngle, { 1, 0, 0 }) * rotation({ 4, 24, 0 }, armSpread, { 0, 0, 1 });
    const QMatrix4x4 rightLeg = rotation({ -2, 12, 0 }, legAngle, { 1, 0, 0 });
    const QMatrix4x4 leftLeg = rotation({ 2, 12, 0 }, -legAngle, { 1, 0, 0 });

    QMatrix4x4 view;
    view.rotate(static_cast<float>(pose.pitch), 1, 0, 0);
    view.rotate(static_cast<float>(pose.yaw), 0, 1, 0);

    QList<Face> faces;
    faces.reserve(96);
    auto part = [&](QVector3D min, QVector3D max, QPointF uv, QPointF overlayUv, QVector3D size, float grow, const QMatrix4x4& partPose) {
        addBox(faces, { min, max, uv, size, 0, partPose, &skin }, view);
        addBox(faces, { min, max, overlayUv, size, grow, partPose, &skin }, view);
    };
    part({ -4, 24, -4 }, { 4, 32, 4 }, { 0, 0 }, { 32, 0 }, { 8, 8, 8 }, 0.5F, body);
    part({ -4, 12, -2 }, { 4, 24, 2 }, { 16, 16 }, { 16, 32 }, { 8, 12, 4 }, 0.25F, body);
    part({ -4 - armWidth, 12, -2 }, { -4, 24, 2 }, { 40, 16 }, { 40, 32 }, { armWidth, 12, 4 }, 0.25F, rightArm);
    part({ 4, 12, -2 }, { 4 + armWidth, 24, 2 }, { 32, 48 }, { 48, 48 }, { armWidth, 12, 4 }, 0.25F, leftArm);
    part({ -4, 0, -2 }, { 0, 12, 2 }, { 0, 16 }, { 0, 32 }, { 4, 12, 4 }, 0.25F, rightLeg);
    part({ 0, 0, -2 }, { 4, 12, 2 }, { 16, 48 }, { 0, 48 }, { 4, 12, 4 }, 0.25F, leftLeg);

    if (!cape.isNull()) {
        // hangs from the shoulders and lifts while walking
        const float lift = static_cast<float>(6 + 12 * pose.stride * (0.6 + 0.4 * std::sin(pose.walk * 2)) + 1.5 * std::sin(pose.idle));
        const QMatrix4x4 capePose = body * rotation({ 0, 24, -2 }, lift, { 1, 0, 0 });
        addBox(faces, { { -5, 8, -3 }, { 5, 24, -2 }, { 0, 0 }, { 10, 16, 1 }, 0, capePose, &cape, cape.width() / 64.0 }, view);
    }

    // facing the viewer and back to front, the overlay sits just outside its part so it wins ties
    QList<Face*> visible;
    visible.reserve(faces.size());
    for (auto& face : faces) {
        const QVector3D normal = QVector3D::crossProduct(face.bottomLeft - face.topLeft, face.topRight - face.topLeft);
        if (normal.z() <= 1e-4F) {
            continue;
        }
        const QVector3D bottomRight = face.topRight + face.bottomLeft - face.topLeft;
        face.depth = (face.topLeft.z() + face.topRight.z() + face.bottomLeft.z() + bottomRight.z()) / 4;
        visible.append(&face);
    }
    std::stable_sort(visible.begin(), visible.end(), [](const Face* a, const Face* b) { return a->depth < b->depth; });

    // the same scale for every pose, so a walking player does not pulse
    const double scale = std::min(rect.height() / 37.0, rect.width() / 24.0);
    const QPointF origin(rect.center().x(), rect.center().y() + 16.5 * scale);
    auto project = [&](const QVector3D& p) { return QPointF(origin.x() + p.x() * scale, origin.y() - p.y() * scale); };

    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter->setRenderHint(QPainter::Antialiasing, false);
    const QTransform base = painter->worldTransform();
    for (const Face* face : visible) {
        const QPointF topLeft = project(face->topLeft);
        const QPointF alongU = (project(face->topRight) - topLeft) / face->texture.width();
        const QPointF alongV = (project(face->bottomLeft) - topLeft) / face->texture.height();
        const QTransform transform(alongU.x(), alongU.y(), alongV.x(), alongV.y(), topLeft.x(), topLeft.y());
        painter->setWorldTransform(transform * base);
        // a third of a screen pixel of overlap hides the seams between faces
        const double lengthU = std::hypot(alongU.x(), alongU.y());
        const double lengthV = std::hypot(alongV.x(), alongV.y());
        const double growU = lengthU > 0 ? 0.35 / lengthU : 0;
        const double growV = lengthV > 0 ? 0.35 / lengthV : 0;
        painter->drawImage(QRectF(-growU, -growV, face->texture.width() + 2 * growU, face->texture.height() + 2 * growV), *face->image,
                           face->texture);
    }
    painter->restore();
}

}  // namespace SkinRenderer
