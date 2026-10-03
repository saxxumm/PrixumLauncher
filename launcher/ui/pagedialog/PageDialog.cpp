/* Copyright 2013-2021 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "PageDialog.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

#include "Application.h"
#include "settings/SettingsObject.h"

#include "ui/themes/NovaIcons.h"
#include "ui/widgets/PageContainer.h"

PageDialog::PageDialog(BasePageProvider* pageProvider, QString defaultId, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(pageProvider->dialogTitle());
    setObjectName("settingsDialog");
    m_container = new PageContainer(pageProvider, std::move(defaultId), this);
    m_container->useSettingsLayout(pageProvider->pageGroups());

    auto* mainLayout = new QVBoxLayout(this);

    auto* focusStealer = new QPushButton(this);
    mainLayout->addWidget(focusStealer);
    focusStealer->setDefault(true);
    focusStealer->hide();

    mainLayout->addWidget(m_container);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    setLayout(mainLayout);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Help | QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("&OK"));
    buttons->button(QDialogButtonBox::Ok)->setProperty("novaRole", "accent");
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("&Cancel"));
    buttons->button(QDialogButtonBox::Help)->setText(tr("Help"));
    buttons->button(QDialogButtonBox::Help)->setIcon(NovaIcons::icon("help", NovaIcons::Tint::Muted));
    // a strip along the bottom, apart from the page
    auto* footer = new QFrame(this);
    footer->setObjectName("pageFooter");
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(4, 10, 16, 12);
    footerLayout->addWidget(buttons);
    m_container->addButtons(footer);

    connect(buttons->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &PageDialog::accept);
    connect(buttons->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &PageDialog::reject);
    connect(buttons->button(QDialogButtonBox::Help), &QPushButton::clicked, m_container, &PageContainer::help);

    restoreGeometry(QByteArray::fromBase64(APPLICATION->settings()->get("PagedGeometry").toString().toUtf8()));
    // windows saved by older versions were smaller than the pages need now
    const QSize preferred(1100, 740);
    if (width() < preferred.width() || height() < preferred.height()) {
        QSize size = preferred.expandedTo(this->size());
        if (auto* screen = this->screen()) {
            size = size.boundedTo(screen->availableSize() * 0.92);
        }
        resize(size);
    }
}

void PageDialog::accept()
{
    if (handleClose())
        QDialog::accept();
}

void PageDialog::closeEvent(QCloseEvent* event)
{
    if (handleClose())
        QDialog::closeEvent(event);
}

bool PageDialog::handleClose()
{
    qDebug() << "Paged dialog close requested";
    if (!m_container->prepareToClose())
        return false;

    qDebug() << "Paged dialog close approved";
    APPLICATION->settings()->set("PagedGeometry", QString::fromUtf8(saveGeometry().toBase64()));
    qDebug() << "Paged dialog geometry saved";

    emit applied();
    return true;
}
