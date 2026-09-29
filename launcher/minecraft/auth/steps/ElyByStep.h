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

#include <QObject>

#include "minecraft/auth/AuthStep.h"
#include "net/NetJob.h"
#include "net/Request.h"

/// logs in to Ely.by with a password or refreshes the stored token
class ElyByStep : public AuthStep {
    Q_OBJECT

   public:
    enum class Mode { Login, Refresh };

    /// the password and the two-factor code are only used for Mode::Login and never stored
    explicit ElyByStep(AccountData* data, Mode mode, QString password = {}, QString totp = {});
    ~ElyByStep() noexcept override = default;

    void perform() override;
    void abort() override;

    QString describe() override;

   private slots:
    void onRequestDone(QByteArray* response);

   private:
    Mode m_mode;
    QString m_password;
    QString m_totp;
    Net::Request::Ptr m_request;
    NetJob::Ptr m_task;
};
