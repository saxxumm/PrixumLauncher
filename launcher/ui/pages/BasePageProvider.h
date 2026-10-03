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

#pragma once

#include <functional>
#include "ui/pages/BasePage.h"

class BasePageProvider {
   public:
    /// a title and the ids of the pages under it
    using PageGroups = QList<std::pair<QString, QStringList>>;

    virtual QList<BasePage*> getPages() = 0;
    virtual QString dialogTitle() = 0;
    /// titles for runs of pages in the page list, none by default
    virtual PageGroups pageGroups() { return {}; }
};

class GenericPageProvider : public BasePageProvider {
    using PageCreator = std::function<BasePage*()>;

   public:
    explicit GenericPageProvider(const QString& dialogTitle) : m_dialogTitle(dialogTitle) {}
    virtual ~GenericPageProvider() {}

    QList<BasePage*> getPages() override
    {
        QList<BasePage*> pages;
        for (PageCreator creator : m_creators) {
            pages.append(creator());
        }
        return pages;
    }
    QString dialogTitle() override { return m_dialogTitle; }

    PageGroups pageGroups() override { return m_pageGroups ? m_pageGroups() : PageGroups(); }

    void setDialogTitle(const QString& title) { m_dialogTitle = title; }
    /// called whenever a dialog opens, so the titles follow the language
    void setPageGroups(std::function<PageGroups()> groups) { m_pageGroups = std::move(groups); }
    void addPageCreator(PageCreator page) { m_creators.append(page); }

    template <typename PageClass>
    void addPage()
    {
        addPageCreator([]() { return new PageClass(); });
    }

   private:
    QList<PageCreator> m_creators;
    QString m_dialogTitle;
    std::function<PageGroups()> m_pageGroups;
};
