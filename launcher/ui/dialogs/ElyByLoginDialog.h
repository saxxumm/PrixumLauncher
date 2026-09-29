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

#include <QDialog>

#include "minecraft/auth/MinecraftAccount.h"

class QDialogButtonBox;
class QLabel;
class QLineEdit;

/// asks for the Ely.by login, password and, when the account has it enabled, the two-factor code
class ElyByLoginDialog : public QDialog {
    Q_OBJECT

   public:
    explicit ElyByLoginDialog(QWidget* parent = nullptr, const QString& username = {});

    /// a logged in account, or nullptr if the user gave up
    static MinecraftAccountPtr newAccount(QWidget* parent, const QString& username = {});

   protected:
    void accept() override;
    void reject() override;
    void changeEvent(QEvent* event) override;

   private:
    void retranslate();
    void updateButtons();
    void setBusy(bool busy);
    void showError(const QString& message);
    void onFailed(const QString& reason);

    QLabel* m_intro = nullptr;
    QLabel* m_usernameLabel = nullptr;
    QLabel* m_passwordLabel = nullptr;
    QLabel* m_codeLabel = nullptr;
    QLineEdit* m_username = nullptr;
    QLineEdit* m_password = nullptr;
    QLineEdit* m_code = nullptr;
    QLabel* m_status = nullptr;
    QDialogButtonBox* m_buttons = nullptr;

    MinecraftAccountPtr m_account;
    shared_qobject_ptr<AuthFlow> m_task;
};
