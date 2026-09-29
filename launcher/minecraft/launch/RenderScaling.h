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

#include <QSize>
#include <QString>
#include <QStringList>

class SettingsObject;

/**
 * Render scaling renders the game at a lower internal resolution and upscales the result to the output size.
 *
 * The game itself is not modified: it is started inside a nested gamescope compositor which owns the
 * low resolution surface and performs the upscale with the selected filter (sharp pixels or smooth blur).
 */
namespace RenderScaling {

struct Config {
    bool enabled = false;
    int percent = 50;
    // nearest | linear | fsr | nis
    QString filter = QStringLiteral("nearest");
    // fit | integer | stretch | fill
    QString scaler = QStringLiteral("fit");
    bool fullscreen = false;
    bool grabCursor = true;
    int sharpness = 2;
    QString extraArgs;
};

struct Plan {
    QSize output;
    QSize internal;
    QString executable;
    QStringList arguments;  // everything up to and including the "--" separator
};

inline constexpr int s_minPercent = 10;
inline constexpr int s_maxPercent = 100;

Config readConfig(SettingsObject* settings);

/// Render scaling is implemented through gamescope, which only exists on Linux
bool isPlatformSupported();

/// @returns absolute path to the gamescope executable or an empty string
QString findGamescope();

/// true when scaling is enabled for these settings and can actually be performed
bool isActive(SettingsObject* settings);

/// the resolution of the final (upscaled) image
QSize outputSize(SettingsObject* settings, const Config& config);

/// the resolution the game renders at
QSize internalSize(const QSize& output, int percent);

QStringList gamescopeArguments(const Config& config, const QSize& output, const QSize& internal);

/// build the full wrapper plan, returns an empty executable when scaling is not active
Plan makePlan(SettingsObject* settings);

/// kill a process and all of its descendants (gamescope does not always take the game down with it)
void killProcessTree(qint64 pid);

}  // namespace RenderScaling
