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

#include "RenderScaling.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include <QScreen>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

#include "settings/SettingsObject.h"

#ifdef Q_OS_LINUX
#include <signal.h>
#include <sys/types.h>
#endif

namespace RenderScaling {

namespace {
const QStringList s_filters{ "nearest", "linear", "fsr", "nis" };
const QStringList s_scalers{ "fit", "integer", "stretch", "fill" };

QString pick(const QString& value, const QStringList& allowed)
{
    const auto lower = value.trimmed().toLower();
    return allowed.contains(lower) ? lower : allowed.first();
}
}  // namespace

Config readConfig(SettingsObject* settings)
{
    Config config;
    config.enabled = settings->get("RenderScaleEnabled").toBool();
    config.percent = std::clamp(settings->get("RenderScalePercent").toInt(), s_minPercent, s_maxPercent);
    config.filter = pick(settings->get("RenderScaleFilter").toString(), s_filters);
    config.scaler = pick(settings->get("RenderScaleMode").toString(), s_scalers);
    config.fullscreen = settings->get("RenderScaleFullscreen").toBool();
    config.grabCursor = settings->get("RenderScaleGrabCursor").toBool();
    config.sharpness = std::clamp(settings->get("RenderScaleSharpness").toInt(), 0, 20);
    config.extraArgs = settings->get("RenderScaleExtraArgs").toString().trimmed();
    return config;
}

bool isPlatformSupported()
{
#ifdef Q_OS_LINUX
    return true;
#else
    return false;
#endif
}

QString findGamescope()
{
    if (!isPlatformSupported()) {
        return {};
    }
    return QStandardPaths::findExecutable(QStringLiteral("gamescope"));
}

bool isActive(SettingsObject* settings)
{
    return settings->get("RenderScaleEnabled").toBool() && !findGamescope().isEmpty();
}

QSize outputSize(SettingsObject* settings, const Config& config)
{
    auto* screen = QGuiApplication::primaryScreen();
    if (screen && config.fullscreen) {
        return (QSizeF(screen->size()) * screen->devicePixelRatio()).toSize();
    }
    if (screen && settings->get("LaunchMaximized").toBool()) {
        return (QSizeF(screen->availableSize()) * screen->devicePixelRatio()).toSize();
    }
    return { std::max(1, settings->get("MinecraftWinWidth").toInt()), std::max(1, settings->get("MinecraftWinHeight").toInt()) };
}

QSize internalSize(const QSize& output, int percent)
{
    const double factor = std::clamp(percent, s_minPercent, s_maxPercent) / 100.0;
    // keep the size even, some drivers dislike odd surface sizes
    auto scale = [factor](int value) { return std::max(64, static_cast<int>(std::lround(value * factor / 2.0)) * 2); };
    return { scale(output.width()), scale(output.height()) };
}

QStringList gamescopeArguments(const Config& config, const QSize& output, const QSize& internal)
{
    QStringList args;
    args << "-w" << QString::number(internal.width()) << "-h" << QString::number(internal.height());
    args << "-W" << QString::number(output.width()) << "-H" << QString::number(output.height());
    args << "-F" << config.filter;
    args << "-S" << config.scaler;
    if (config.filter == "fsr" || config.filter == "nis") {
        args << "--sharpness" << QString::number(config.sharpness);
    }
    if (config.fullscreen) {
        args << "-f";
    }
    if (config.grabCursor) {
        args << "--force-grab-cursor";
    }
    if (!config.extraArgs.isEmpty()) {
        args << QProcess::splitCommand(config.extraArgs);
    }
    args << "--";
    return args;
}

Plan makePlan(SettingsObject* settings)
{
    Plan plan;
    const auto config = readConfig(settings);
    if (!config.enabled) {
        return plan;
    }
    plan.output = outputSize(settings, config);
    plan.internal = internalSize(plan.output, config.percent);
    plan.executable = findGamescope();
    if (!plan.executable.isEmpty()) {
        plan.arguments = gamescopeArguments(config, plan.output, plan.internal);
    }
    return plan;
}

void killProcessTree(qint64 pid)
{
#ifdef Q_OS_LINUX
    if (pid <= 0) {
        return;
    }
    QList<qint64> found{ pid };
    for (qsizetype i = 0; i < found.size(); i++) {
        // every thread keeps its own list of children
        const QDir tasks(QString("/proc/%1/task").arg(found[i]));
        for (const auto& task : tasks.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QFile children(tasks.filePath(task + "/children"));
            if (!children.open(QIODevice::ReadOnly)) {
                continue;
            }
            for (const auto& child : QString::fromLatin1(children.readAll()).simplified().split(' ', Qt::SkipEmptyParts)) {
                bool ok = false;
                const auto childPid = child.toLongLong(&ok);
                if (ok && !found.contains(childPid)) {
                    found << childPid;
                }
            }
        }
    }
    // children first, so nothing gets a chance to respawn
    for (auto it = found.crbegin(); it != found.crend(); ++it) {
        ::kill(static_cast<pid_t>(*it), SIGKILL);
    }
#else
    Q_UNUSED(pid)
#endif
}

}  // namespace RenderScaling
