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

#include "ThemeEditorDialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "Application.h"
#include "DesktopServices.h"
#include "settings/SettingsObject.h"
#include "ui/themes/NovaIcons.h"
#include "ui/themes/ThemeManager.h"

namespace {

QWidget* scrollable(QWidget* content, QWidget* parent)
{
    auto* scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    return scroll;
}

QLabel* sectionLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text.toUpper(), parent);
    label->setProperty("novaRole", "section");
    return label;
}

}  // namespace

ThemeEditorDialog::ThemeEditorDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Theme Editor"));
    resize(1080, 720);

    m_applyTimer.setSingleShot(true);
    m_applyTimer.setInterval(120);
    connect(&m_applyTimer, &QTimer::timeout, this, &ThemeEditorDialog::applyNow);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 12);
    root->setSpacing(12);

    // name and starting point
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        m_name = new QLineEdit(this);
        m_name->setPlaceholderText(tr("My Theme"));
        m_name->setMinimumWidth(220);
        m_base = new QComboBox(this);
        m_base->setMinimumWidth(200);
        for (auto* theme : APPLICATION->themeManager()->getValidApplicationThemes()) {
            if (auto* nova = dynamic_cast<NovaTheme*>(theme)) {
                m_base->addItem(nova->name(), nova->id());
            }
        }
        row->addWidget(new QLabel(tr("&Name:"), this));
        qobject_cast<QLabel*>(row->itemAt(0)->widget())->setBuddy(m_name);
        row->addWidget(m_name);
        row->addStretch(1);
        auto* baseLabel = new QLabel(tr("&Start from:"), this);
        baseLabel->setBuddy(m_base);
        row->addWidget(baseLabel);
        row->addWidget(m_base);
        root->addLayout(row);

        connect(m_base, &QComboBox::activated, this, [this](int index) { loadTheme(m_base->itemData(index).toString()); });
    }

    // editor and preview
    {
        auto* body = new QHBoxLayout;
        body->setSpacing(14);
        auto* tabs = new QTabWidget(this);
        tabs->setMinimumWidth(450);
        tabs->setMaximumWidth(500);
        tabs->addTab(scrollable(createColorsPage(), tabs), NovaIcons::icon("palette"), tr("Colors"));
        tabs->addTab(scrollable(createShapePage(), tabs), NovaIcons::icon("grid-small"), tr("Shape && Size"));
        tabs->addTab(createStylesheetPage(), NovaIcons::icon("logs"), tr("Stylesheet"));
        body->addWidget(tabs);
        body->addWidget(createPreview(), 1);
        root->addLayout(body, 1);
    }

    m_status = new QLabel(this);
    m_status->setProperty("novaRole", "muted");
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_status->setText(tr("Changes are applied to the whole launcher right away. Themes are saved as theme.json and style.qss in "
                         "%1. You can also edit those files in any text editor, the launcher reloads them as soon as they change.")
                          .arg(APPLICATION->themeManager()->getApplicationThemesFolder().absolutePath()));
    root->addWidget(m_status);

    // buttons
    {
        auto* row = new QHBoxLayout;
        auto* openFolder = new QPushButton(NovaIcons::icon("folder"), tr("Open Themes &Folder"), this);
        auto* exportButton = new QPushButton(NovaIcons::icon("export"), tr("E&xport..."), this);
        auto* cancel = new QPushButton(tr("Cancel"), this);
        auto* saveAsNew = new QPushButton(tr("Save as &New"), this);
        auto* saveButton = new QPushButton(tr("&Save"), this);
        saveButton->setDefault(true);
        row->addWidget(openFolder);
        row->addWidget(exportButton);
        row->addStretch(1);
        row->addWidget(cancel);
        row->addWidget(saveAsNew);
        row->addWidget(saveButton);
        root->addLayout(row);

        connect(openFolder, &QPushButton::clicked, this,
                [] { DesktopServices::openPath(APPLICATION->themeManager()->getApplicationThemesFolder().path(), true); });
        connect(exportButton, &QPushButton::clicked, this, &ThemeEditorDialog::exportTheme);
        connect(cancel, &QPushButton::clicked, this, &ThemeEditorDialog::reject);
        connect(saveAsNew, &QPushButton::clicked, this, [this] {
            if (save(true)) {
                accept();
            }
        });
        connect(saveButton, &QPushButton::clicked, this, [this] {
            if (save(false)) {
                accept();
            }
        });
    }

    auto* current = APPLICATION->themeManager()->currentNovaTheme();
    loadTheme(current ? current->id() : QStringLiteral("nova-prixum"));
}

QWidget* ThemeEditorDialog::createColorsPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 4, 12, 12);

    QString group;
    QFormLayout* form = nullptr;
    auto addColorRow = [this, &form, page](const QString& key, const QString& label, bool derived) {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);
        auto* swatch = new QToolButton(page);
        swatch->setObjectName("novaSwatch");
        swatch->setFixedSize(34, 24);
        swatch->setCursor(Qt::PointingHandCursor);
        auto* hex = new QLineEdit(page);
        hex->setFixedWidth(110);
        if (derived) {
            hex->setPlaceholderText(tr("automatic"));
            hex->setToolTip(tr("Leave empty to calculate this color from the base colors."));
        }
        row->addWidget(swatch);
        row->addWidget(hex);
        row->addStretch(1);
        form->addRow(label, row);
        m_swatches[key] = swatch;
        m_hexEdits[key] = hex;

        connect(swatch, &QToolButton::clicked, this, [this, key, label] {
            const QColor chosen =
                QColorDialog::getColor(m_tokens.color(key), this, tr("Choose %1").arg(label), QColorDialog::ShowAlphaChannel);
            if (chosen.isValid()) {
                setColor(key, chosen);
            }
        });
        connect(hex, &QLineEdit::editingFinished, this, [this, key, hex, derived] {
            const QString text = hex->text().trimmed();
            if (text.isEmpty() && derived) {
                setColor(key, QColor());
                return;
            }
            const QColor color = QColor::fromString(text);
            if (color.isValid()) {
                setColor(key, color);
            } else {
                setColor(key, m_tokens.colors.value(key));
            }
        });
    };

    // one form for everything keeps the columns aligned
    form = new QFormLayout;
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(6);
    layout->addLayout(form);
    for (const auto& token : Nova::colorTokens()) {
        if (token.group != group) {
            group = token.group;
            form->addRow(sectionLabel(group, page));
        }
        addColorRow(token.key, token.label, false);
    }

    form->addRow(sectionLabel(tr("Fine tuning"), page));
    auto* hint = new QLabel(tr("These are calculated from the colors above unless you set them yourself."), page);
    hint->setProperty("novaRole", "muted");
    hint->setWordWrap(true);
    form->addRow(hint);
    for (const auto& key : Nova::derivedColorKeys()) {
        addColorRow(key, key, true);
    }
    layout->addStretch(1);
    return page;
}

QWidget* ThemeEditorDialog::createShapePage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 4, 12, 12);
    layout->addWidget(sectionLabel(tr("Shape and size"), page));

    auto* form = new QFormLayout;
    form->setVerticalSpacing(8);
    for (const auto& token : Nova::metricTokens()) {
        auto* row = new QHBoxLayout;
        auto* slider = new QSlider(Qt::Horizontal, page);
        slider->setRange(token.min, token.max);
        auto* spin = new QSpinBox(page);
        spin->setRange(token.min, token.max);
        spin->setSuffix(" " + token.suffix);
        spin->setFixedWidth(90);
        row->addWidget(slider, 1);
        row->addWidget(spin);
        form->addRow(token.label, row);
        m_metrics[token.key] = spin;

        connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
        connect(spin, &QSpinBox::valueChanged, slider, &QSlider::setValue);
        connect(spin, &QSpinBox::valueChanged, this, [this, key = token.key](int value) {
            if (!m_loading) {
                m_tokens.metrics[key] = value;
                scheduleApply();
            }
        });
    }
    layout->addLayout(form);

    layout->addWidget(sectionLabel(tr("Font"), page));
    m_systemFont = new QCheckBox(tr("Use the system font"), page);
    m_font = new QFontComboBox(page);
    layout->addWidget(m_systemFont);
    layout->addWidget(m_font);
    connect(m_systemFont, &QCheckBox::toggled, m_font, &QWidget::setDisabled);
    auto updateFont = [this] {
        if (!m_loading) {
            m_tokens.fontFamily = m_systemFont->isChecked() ? QString() : m_font->currentFont().family();
            scheduleApply();
        }
    };
    connect(m_systemFont, &QCheckBox::toggled, this, updateFont);
    connect(m_font, &QFontComboBox::currentFontChanged, this, updateFont);

    layout->addStretch(1);
    return page;
}

QWidget* ThemeEditorDialog::createStylesheetPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 10, 12, 12);

    auto* help = new QLabel(tr("Qt stylesheet rules added on top of the base style. Every token can be used, for example "
                               "<code>@accent</code>, <code>@surface</code>, <code>@radius</code> or <code>@padY</code>."),
                            page);
    help->setWordWrap(true);
    layout->addWidget(help);

    m_qss = new QPlainTextEdit(page);
    QFont mono("monospace");
    mono.setStyleHint(QFont::Monospace);
    m_qss->setFont(mono);
    m_qss->setPlaceholderText("QPushButton {\n    border-radius: @radius;\n}\n\nQFrame#novaSidebar {\n    background: @surface;\n}");
    m_qss->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(m_qss, 1);

    m_replaceBase = new QCheckBox(tr("Replace the base style completely"), page);
    m_replaceBase->setToolTip(tr("Only your stylesheet is used. Insert the base style first to start from a copy of it."));
    layout->addWidget(m_replaceBase);

    auto* row = new QHBoxLayout;
    auto* insertBase = new QPushButton(tr("Insert Base Style"), page);
    insertBase->setToolTip(tr("Copy the complete built-in stylesheet into the editor."));
    auto* apply = new QPushButton(tr("&Apply"), page);
    row->addWidget(insertBase);
    row->addStretch(1);
    row->addWidget(apply);
    layout->addLayout(row);

    auto* tokenList = new QLabel(page);
    tokenList->setProperty("novaRole", "muted");
    tokenList->setWordWrap(true);
    tokenList->setTextInteractionFlags(Qt::TextSelectableByMouse);
    tokenList->setText(tr("Tokens: %1").arg("@" + Nova::Tokens::defaults().variables().keys().join(", @")));
    layout->addWidget(tokenList);

    connect(insertBase, &QPushButton::clicked, this, [this] {
        if (!m_qss->toPlainText().trimmed().isEmpty() &&
            QMessageBox::question(this, tr("Insert base style"), tr("This replaces the current contents of the editor. Continue?")) !=
                QMessageBox::Yes) {
            return;
        }
        m_qss->setPlainText(Nova::baseStyleSheet());
        m_replaceBase->setChecked(true);
    });
    connect(apply, &QPushButton::clicked, this, &ThemeEditorDialog::applyNow);
    connect(m_replaceBase, &QCheckBox::toggled, this, &ThemeEditorDialog::scheduleApply);
    connect(m_qss, &QPlainTextEdit::textChanged, this, [this] {
        if (!m_loading) {
            // typing produces broken rules on the way, give it some time
            m_applyTimer.start(700);
        }
    });
    return page;
}

QWidget* ThemeEditorDialog::createPreview()
{
    auto* card = new QFrame(this);
    card->setObjectName("novaPreviewCard");
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 18);
    layout->setSpacing(10);

    auto* title = new QLabel(tr("Preview"), card);
    title->setObjectName("pageHeader");
    layout->addWidget(title);
    auto* subtitle =
        new QLabel(tr("Every window of the launcher follows these settings, including the main window behind this dialog."), card);
    subtitle->setProperty("novaRole", "muted");
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    auto* search = new QLineEdit(card);
    search->setPlaceholderText(tr("Search mods..."));
    search->addAction(NovaIcons::icon("search", NovaIcons::Tint::Muted), QLineEdit::LeadingPosition);
    auto* combo = new QComboBox(card);
    combo->addItems({ "Fabric 0.16.9", "Forge 52.0.1", "NeoForge 21.1.77", "Quilt 0.27.1" });
    auto* inputs = new QHBoxLayout;
    inputs->addWidget(search, 1);
    inputs->addWidget(combo);
    layout->addLayout(inputs);

    auto* checks = new QHBoxLayout;
    auto* check = new QCheckBox(tr("Enable mods"), card);
    check->setChecked(true);
    auto* radioA = new QRadioButton(tr("Release"), card);
    radioA->setChecked(true);
    auto* radioB = new QRadioButton(tr("Snapshot"), card);
    checks->addWidget(check);
    checks->addWidget(radioA);
    checks->addWidget(radioB);
    checks->addStretch(1);
    layout->addLayout(checks);

    auto* slider = new QSlider(Qt::Horizontal, card);
    slider->setRange(0, 100);
    slider->setValue(62);
    auto* progress = new QProgressBar(card);
    progress->setValue(68);
    layout->addWidget(slider);
    layout->addWidget(progress);

    auto* buttons = new QHBoxLayout;
    auto* secondary = new QPushButton(NovaIcons::icon("folder"), tr("Folder"), card);
    auto* primary = new QPushButton(NovaIcons::icon("play", NovaIcons::Tint::AccentText), tr("Play"), card);
    primary->setProperty("novaRole", "primary");
    auto* disabled = new QPushButton(tr("Disabled"), card);
    disabled->setEnabled(false);
    buttons->addWidget(secondary);
    buttons->addWidget(primary);
    buttons->addWidget(disabled);
    buttons->addStretch(1);
    layout->addLayout(buttons);

    auto* tabs = new QTabWidget(card);
    auto* list = new QListWidget(tabs);
    for (const auto& [icon, name] : std::initializer_list<std::pair<QString, QString>>{
             { "cube", "Sodium" }, { "sparkles", "Iris Shaders" }, { "grid-small", "Lithium" }, { "history", "Mod Menu" } }) {
        auto* item = new QListWidgetItem(NovaIcons::icon(icon), name, list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(name == "Lithium" ? Qt::Unchecked : Qt::Checked);
    }
    list->setCurrentRow(1);
    tabs->addTab(list, tr("Mods"));
    auto* group = new QGroupBox(tr("Memory"), tabs);
    group->setCheckable(true);
    auto* groupLayout = new QFormLayout(group);
    auto* spin = new QSpinBox(group);
    spin->setRange(512, 32768);
    spin->setValue(4096);
    spin->setSuffix(" MiB");
    groupLayout->addRow(tr("Maximum:"), spin);
    auto* groupPage = new QWidget(tabs);
    auto* groupPageLayout = new QVBoxLayout(groupPage);
    groupPageLayout->addWidget(group);
    groupPageLayout->addStretch(1);
    tabs->addTab(groupPage, tr("Settings"));
    layout->addWidget(tabs, 1);

    return card;
}

void ThemeEditorDialog::loadTheme(const QString& id)
{
    auto* theme = APPLICATION->themeManager()->novaTheme(id);
    if (!theme) {
        theme = APPLICATION->themeManager()->novaTheme("nova-prixum");
    }
    m_loading = true;
    if (theme) {
        m_themeId = theme->id();
        m_name->setText(theme->isBuiltIn() ? tr("%1 (Custom)").arg(theme->name()) : theme->name());
        m_qss->setPlainText(theme->customQss());
        m_replaceBase->setChecked(theme->replacesBaseQss());
        m_base->setCurrentIndex(std::max(0, m_base->findData(theme->id())));
        setTokens(theme->tokens());
    } else {
        m_themeId.clear();
        setTokens(Nova::Tokens::defaults());
    }
    m_loading = false;
    scheduleApply();
}

void ThemeEditorDialog::setTokens(const Nova::Tokens& tokens)
{
    const bool wasLoading = m_loading;
    m_loading = true;
    m_tokens = tokens;
    for (auto it = m_swatches.cbegin(); it != m_swatches.cend(); ++it) {
        setColor(it.key(), m_tokens.colors.value(it.key()));
    }
    for (auto it = m_metrics.cbegin(); it != m_metrics.cend(); ++it) {
        it.value()->setValue(m_tokens.metric(it.key()));
    }
    m_systemFont->setChecked(m_tokens.fontFamily.isEmpty());
    m_font->setDisabled(m_tokens.fontFamily.isEmpty());
    if (!m_tokens.fontFamily.isEmpty()) {
        m_font->setCurrentFont(QFont(m_tokens.fontFamily));
    }
    m_loading = wasLoading;
}

void ThemeEditorDialog::setColor(const QString& key, const QColor& color)
{
    const bool derived = Nova::derivedColorKeys().contains(key);
    if (color.isValid()) {
        m_tokens.colors[key] = color;
    } else if (derived) {
        m_tokens.colors.remove(key);
    }

    const QColor effective = m_tokens.color(key);
    if (auto* swatch = m_swatches.value(key)) {
        swatch->setStyleSheet(QString("QToolButton#novaSwatch { background: %1; }").arg(effective.name(QColor::HexArgb)));
    }
    if (auto* hex = m_hexEdits.value(key)) {
        const bool explicitColor = m_tokens.colors.contains(key);
        hex->setText(!explicitColor ? QString()
                                    : (effective.alpha() == 255 ? effective.name(QColor::HexRgb) : effective.name(QColor::HexArgb)));
    }
    if (!m_loading) {
        // derived colors depend on the base ones
        for (const auto& other : Nova::derivedColorKeys()) {
            if (other != key && !m_tokens.colors.contains(other)) {
                if (auto* swatch = m_swatches.value(other)) {
                    swatch->setStyleSheet(
                        QString("QToolButton#novaSwatch { background: %1; }").arg(m_tokens.color(other).name(QColor::HexArgb)));
                }
            }
        }
        scheduleApply();
    }
}

void ThemeEditorDialog::scheduleApply()
{
    if (!m_loading) {
        m_applyTimer.start(120);
    }
}

void ThemeEditorDialog::applyNow()
{
    m_applyTimer.stop();
    NovaTheme::applyTokens(m_tokens, m_qss->toPlainText(), m_replaceBase->isChecked());
    APPLICATION->themeManager()->refreshIconTheme();
    emit APPLICATION->themeApplied();
}

bool ThemeEditorDialog::save(bool asNew)
{
    auto* manager = APPLICATION->themeManager();
    QString name = m_name->text().trimmed();
    if (name.isEmpty()) {
        name = tr("My Theme");
    }

    QString id = m_themeId;
    auto* existing = manager->novaTheme(id);
    if (asNew || !existing || existing->isBuiltIn()) {
        id = manager->uniqueThemeFolderName(name);
    }
    const QString directory = manager->getApplicationThemesFolder().absoluteFilePath(id);
    if (auto result = NovaTheme::save(directory, name, m_tokens, m_qss->toPlainText(), m_replaceBase->isChecked()); !result) {
        QMessageBox::warning(this, tr("Couldn't save the theme"), result.error());
        return false;
    }

    manager->refresh();
    APPLICATION->settings()->set("ApplicationTheme", id);
    manager->applyCurrentlySelectedTheme();
    m_themeId = id;
    return true;
}

void ThemeEditorDialog::exportTheme()
{
    const QString target = QFileDialog::getExistingDirectory(this, tr("Export theme into folder"));
    if (target.isEmpty()) {
        return;
    }
    QString name = m_name->text().trimmed();
    if (name.isEmpty()) {
        name = tr("My Theme");
    }
    const QString folder = QDir(target).absoluteFilePath(APPLICATION->themeManager()->uniqueThemeFolderName(name));
    if (auto result = NovaTheme::save(folder, name, m_tokens, m_qss->toPlainText(), m_replaceBase->isChecked()); !result) {
        QMessageBox::warning(this, tr("Couldn't export the theme"), result.error());
        return;
    }
    QMessageBox::information(this, tr("Theme exported"),
                             tr("The theme was exported to %1.\nCopy this folder into the themes folder of any launcher to use it.")
                                 .arg(QDir::toNativeSeparators(folder)));
}

void ThemeEditorDialog::reject()
{
    // throw the live preview away
    APPLICATION->themeManager()->applyCurrentlySelectedTheme();
    QDialog::reject();
}
