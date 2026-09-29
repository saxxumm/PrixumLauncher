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

#include <QDomDocument>
#include <QTest>

#include <news/NewsEntry.h>

class NewsEntryTest : public QObject {
    Q_OBJECT

    // NewsEntry is a QObject and can't be returned by value
    struct Parsed {
        QString title;
        QString content;
        QString link;
    };

    static Parsed parse(const QString& xml)
    {
        QDomDocument doc;
        doc.setContent(xml);
        NewsEntry entry;
        QString error;
        NewsEntry::fromXmlElement(doc.documentElement(), &entry, &error);
        return { entry.title, entry.content, entry.link };
    }

   private slots:
    void test_githubReleaseUsesAlternateLink()
    {
        // shape of an entry in https://github.com/<owner>/<repo>/releases.atom
        auto entry = parse(
            "<entry>"
            "<id>tag:github.com,2008:Repository/1/12.1.0</id>"
            "<link rel='alternate' type='text/html' href='https://github.com/owner/repo/releases/tag/12.1.0'/>"
            "<title>Prixum Launcher 12.1.0</title>"
            "<content type='html'>&lt;p&gt;Notes&lt;/p&gt;</content>"
            "</entry>");
        QCOMPARE(entry.title, QString("Prixum Launcher 12.1.0"));
        QCOMPARE(entry.link, QString("https://github.com/owner/repo/releases/tag/12.1.0"));
        QCOMPARE(entry.content, QString("<p>Notes</p>"));
    }

    void test_linkWithoutRelIsAlternate()
    {
        auto entry = parse("<entry><id>urn:x</id><link href='https://example.org/post'/><title>Post</title></entry>");
        QCOMPARE(entry.link, QString("https://example.org/post"));
    }

    void test_otherLinksAreIgnored()
    {
        auto entry = parse(
            "<entry>"
            "<id>https://example.org/post</id>"
            "<link rel='enclosure' href='https://example.org/image.png'/>"
            "<title>Post</title>"
            "</entry>");
        QCOMPARE(entry.link, QString("https://example.org/post"));
    }

    void test_idIsTheLinkWithoutLinkElements()
    {
        // Prism Launcher's own feed keeps the page URL in the id
        auto entry = parse("<entry><id>https://example.org/news/post</id><title>Post</title></entry>");
        QCOMPARE(entry.link, QString("https://example.org/news/post"));
    }
};

QTEST_GUILESS_MAIN(NewsEntryTest)

#include "NewsEntry_test.moc"
