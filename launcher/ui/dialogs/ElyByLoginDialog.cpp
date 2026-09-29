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

#include "ElyByLoginDialog.h"

#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

#include "minecraft/auth/AuthFlow.h"

ElyByLoginDialog::ElyByLoginDialog(QWidget* parent, const QString& username) : QDialog(parent)
{
    setMinimumWidth(440);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    m_intro = new QLabel(this);
    m_intro->setWordWrap(true);
    m_intro->setOpenExternalLinks(true);
    m_intro->setTextFormat(Qt::RichText);
    layout->addWidget(m_intro);

    auto* form = new QFormLayout;
    m_username = new QLineEdit(username, this);
    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setInputMethodHints(Qt::ImhHiddenText | Qt::ImhNoPredictiveText | Qt::ImhSensitiveData);
    m_code = new QLineEdit(this);
    m_code->setValidator(new QRegularExpressionValidator(QRegularExpression("\\d{0,8}"), m_code));
    m_code->setInputMethodHints(Qt::ImhDigitsOnly);
    m_usernameLabel = new QLabel(this);
    m_passwordLabel = new QLabel(this);
    m_codeLabel = new QLabel(this);
    m_usernameLabel->setBuddy(m_username);
    m_passwordLabel->setBuddy(m_password);
    m_codeLabel->setBuddy(m_code);
    form->addRow(m_usernameLabel, m_username);
    form->addRow(m_passwordLabel, m_password);
    form->addRow(m_codeLabel, m_code);
    layout->addLayout(form);
    // only shown once Ely.by asks for it
    m_codeLabel->hide();
    m_code->hide();

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setProperty("novaRole", "muted");
    m_status->hide();
    layout->addWidget(m_status);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(m_buttons);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &ElyByLoginDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &ElyByLoginDialog::reject);

    for (auto* field : { m_username, m_password, m_code }) {
        connect(field, &QLineEdit::textChanged, this, &ElyByLoginDialog::updateButtons);
    }

    retranslate();
    updateButtons();
    (username.isEmpty() ? m_username : m_password)->setFocus();
}

MinecraftAccountPtr ElyByLoginDialog::newAccount(QWidget* parent, const QString& username)
{
    ElyByLoginDialog dialog(parent, username);
    if (dialog.exec() == QDialog::Accepted) {
        return dialog.m_account;
    }
    return nullptr;
}

void ElyByLoginDialog::retranslate()
{
    setWindowTitle(tr("Add Ely.by account"));
    m_intro->setText(
        tr("Log in with your <a href=\"https://ely.by\">Ely.by</a> account to play on servers that use Ely.by and to "
           "show your Ely.by skin. The password goes to Ely.by only and isn't saved.<br><br>"
           "No account yet? <a href=\"https://account.ely.by/register\">Create one on ely.by</a>."));
    m_usernameLabel->setText(tr("&Nickname or e-mail:"));
    m_passwordLabel->setText(tr("&Password:"));
    m_codeLabel->setText(tr("Two-factor &code:"));
    m_code->setPlaceholderText(tr("6 digits from your authenticator app"));
    m_buttons->button(QDialogButtonBox::Ok)->setText(tr("&Log in"));
}

void ElyByLoginDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
    QDialog::changeEvent(event);
}

void ElyByLoginDialog::updateButtons()
{
    const bool codeReady = m_code->isHidden() || m_code->text().size() >= 6;
    const bool ready = !m_task && !m_username->text().trimmed().isEmpty() && !m_password->text().isEmpty() && codeReady;
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(ready);
}

void ElyByLoginDialog::setBusy(bool busy)
{
    for (auto* field : { m_username, m_password, m_code }) {
        field->setEnabled(!busy);
    }
    if (busy) {
        m_status->setText(tr("Logging in to Ely.by..."));
        m_status->show();
    }
    updateButtons();
}

void ElyByLoginDialog::showError(const QString& message)
{
    m_status->setText(message);
    m_status->show();
}

void ElyByLoginDialog::accept()
{
    if (m_task) {
        return;
    }
    m_account = MinecraftAccount::createElyBy(m_username->text().trimmed());
    m_task = m_account->loginElyBy(m_password->text(), m_code->isHidden() ? QString() : m_code->text());
    connect(m_task.get(), &Task::succeeded, this, [this] {
        m_task.reset();
        QDialog::accept();
    });
    connect(m_task.get(), &Task::failed, this, &ElyByLoginDialog::onFailed);
    setBusy(true);
    m_task->start();
}

void ElyByLoginDialog::onFailed(const QString& reason)
{
    m_task.reset();
    setBusy(false);
    if (m_account->accountData()->twoFactorRequired) {
        m_codeLabel->show();
        m_code->show();
        m_code->clear();
        m_code->setFocus();
        showError(tr("This account is protected with two-factor authentication. Enter the code from your authenticator app."));
    } else {
        showError(reason);
        m_password->selectAll();
        m_password->setFocus();
    }
    m_account.reset();
    updateButtons();
}

void ElyByLoginDialog::reject()
{
    if (m_task) {
        m_task->abort();
        m_task.reset();
    }
    m_account.reset();
    QDialog::reject();
}
