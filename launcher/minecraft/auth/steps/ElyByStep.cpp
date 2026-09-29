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

#include "ElyByStep.h"

#include <QUuid>

#include "Application.h"
#include "Logging.h"
#include "minecraft/auth/ElyBy.h"
#include "net/NetUtils.h"
#include "net/RawHeaderProxy.h"

ElyByStep::ElyByStep(AccountData* data, Mode mode, QString password, QString totp)
    : AuthStep(data), m_mode(mode), m_password(std::move(password)), m_totp(std::move(totp))
{}

QString ElyByStep::describe()
{
    return m_mode == Mode::Login ? tr("Logging in to Ely.by") : tr("Refreshing the Ely.by session");
}

void ElyByStep::perform()
{
    m_data->twoFactorRequired = false;

    auto& token = m_data->yggdrasilToken;
    if (token.extra["clientToken"].toString().isEmpty()) {
        token.extra["clientToken"] = QUuid::createUuid().toString(QUuid::Id128);
    }
    const auto clientToken = token.extra["clientToken"].toString();

    QUrl url;
    QByteArray body;
    if (m_mode == Mode::Login) {
        url = ElyBy::authenticateUrl();
        // Ely.by expects the two-factor code appended to the password
        const auto password = m_totp.isEmpty() ? m_password : m_password + ':' + m_totp;
        body = ElyBy::authenticateRequest(token.extra["userName"].toString(), password, clientToken);
    } else {
        if (token.token.isEmpty()) {
            emit finished(AccountTaskState::STATE_FAILED_HARD, tr("The Ely.by session has ended, log in again."));
            return;
        }
        url = ElyBy::refreshUrl();
        body = ElyBy::refreshRequest(token.token, clientToken);
    }

    auto headers = QList<Net::HeaderPair>{
        { "Content-Type", "application/json" },
        { "Accept", "application/json" },
    };
    auto [request, response] = Net::Request::makeByteArray(url, body);
    m_request = request;
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    m_request->enableAutoRetry(true);

    m_task.reset(new NetJob("ElyByStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });
    m_task->start();
}

void ElyByStep::abort()
{
    if (m_task) {
        m_task->abort();
    }
}

void ElyByStep::onRequestDone(QByteArray* response)
{
    // the reply holds the access token
    qCDebug(authCredentials()) << *response;

    const auto error = m_request->error();
    if (error != QNetworkReply::NoError) {
        qWarning() << "Ely.by reply error:" << error;
        if (!Net::isApplicationError(error) || Net::isServerError(error)) {
            m_data->networkError = error;
            emit finished(AccountTaskState::STATE_OFFLINE, tr("Couldn't reach Ely.by: %1").arg(m_request->errorString()));
            return;
        }

        const auto elyError = ElyBy::parseError(*response);
        if (m_mode == Mode::Login && ElyBy::isTwoFactorRequired(elyError)) {
            m_data->twoFactorRequired = true;
            emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Enter the code from your authenticator app."));
            return;
        }
        QString message;
        if (ElyBy::isInvalidCredentials(elyError)) {
            message = m_totp.isEmpty() ? tr("Wrong login or password.") : tr("Wrong login, password or code.");
        } else if (m_mode == Mode::Refresh) {
            message = tr("The Ely.by session has ended, log in again.");
        } else {
            message = elyError.message.isEmpty() ? m_request->errorString() : elyError.message;
        }
        // a rejected refresh means the token is gone for good
        emit finished(AccountTaskState::STATE_FAILED_HARD, message);
        return;
    }

    if (!ElyBy::parseAuthResponse(*response, *m_data)) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Ely.by sent an unexpected response."));
        return;
    }
    emit finished(AccountTaskState::STATE_WORKING, tr("Logged in to Ely.by"));
}
