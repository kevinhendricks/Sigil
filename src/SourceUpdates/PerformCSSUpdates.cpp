/************************************************************************
**
**  Copyright (C) 2015-2021 Kevin B. Hendricks, Stratford, Ontario, Canada
**  Copyright (C) 2021 Doug Massay
**  Copyright (C) 2009-2011 Strahinja Markovic  <strahinja.markovic@gmail.com>
**
**  This file is part of Sigil.
**
**  Sigil is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  Sigil is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with Sigil.  If not, see <http://www.gnu.org/licenses/>.
**
*************************************************************************/

#include <QDir>
#include <QUrl>
#include <QFileInfo>
#include <QString>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QDebug>

#include "Misc/Utility.h"
#include "SourceUpdates/PerformCSSUpdates.h"

// Expression debugging
#define DBG if(0)
// Start index/increment debugging
#define DBG2 if(0)

static const QChar FORWARD_SLASH = QChar::fromLatin1('/');

PerformCSSUpdates::PerformCSSUpdates(const QString &source, 
                     const QString& newbookpath, 
                     const QHash<QString, QString> &css_updates, 
                     const QString &currentpath)
    :
    m_Source(source),
    m_CSSUpdates(css_updates),
    m_CurrentPath(currentpath),
    m_newbookpath(newbookpath)
{
}


QString PerformCSSUpdates::operator()()
{
    QString result(m_Source);
    QString origDir = QFileInfo(m_CurrentPath).dir().path();
    QString destfile = QFileInfo(m_newbookpath).fileName();
    const QList<QString> &keys = m_CSSUpdates.keys();
    int num_keys = keys.count();
    if (num_keys == 0) return result;

    // Now parse the text once looking for keys and replacing them where needed
    QRegularExpression reference(
        "(?:(?:src|background|background-image|block|border|border-image|border-image-source|"
        "content|cursor|list-style|list-style-image|mask|mask-image|(?:-webkit-)?shape-outside)\\s*:|"
        "@import)\\s*"
        "("
        "[^;\\}]*"
        ")"
        "(?:;|\\})");

    QRegularExpression urls(
        "(?:"
        "url\\([\"']?([^\\(\\)\"']*)[\"']?\\)"
        ")");

    QRegularExpression importurls(
        "(?:"
        "url\\([\"']?([^\\(\\)\"']*)[\"']?\\)"
        "|"
        "[\"']([^\\(\\)\"']*)[\"']"
        ")");

    int start_index = 0;
    int start_index_correction = 0;
    QRegularExpressionMatch mo = reference.match(result, start_index);
    // handle case if no initial match at all
    if (!mo.hasMatch()) return result;
    do {
        bool changes_made = false;
        for (int i = 1; i <= reference.captureCount(); ++i) {
            if (mo.captured(i).trimmed().isEmpty()) {
                continue;
            }
            DBG2 qDebug() << "start index is: " << QString::number(start_index);
            DBG2 qDebug() << "i is: " << QString::number(i);
            DBG2 qDebug() << mo.captured(i) << " start: " << mo.capturedStart(i) << " len: " << mo.capturedLength(i);
            // Check the captured property attribute string fragment for multiple urls
            int frag_start_index = 0;
            int frag_index_correction = 0;
            QString fragment = mo.captured(i);
            if(mo.captured().startsWith("@import", Qt::CaseInsensitive)) {
                DBG qDebug() << "Using alternate expression for @import urls";
                DBG qDebug() << mo.captured();
                urls.swap(importurls);
            }
            QRegularExpressionMatch frag_mo = urls.match(fragment, frag_start_index);
            // only loop if at least one match was found
            if (frag_mo.hasMatch()) {
                DBG2 qDebug() << "urls.matches found " << QString::number(urls.captureCount());
                do {
                    for (int j = 1; j <= urls.captureCount(); ++j) {
                        DBG2 qDebug() << "processing url match number: " << QString::number(j);
                        DBG2 qDebug() << "frag_mo is: " << frag_mo.captured(j);
                        if (frag_mo.captured(j).trimmed().isEmpty()) {
                            continue;
                        }
                        QString apath = Utility::URLDecodePath(frag_mo.captured(j));
                        QString dest_oldbkpath = Utility::buildBookPath(apath, origDir);
                        // targets may not have moved but we may have
                        QString dest_newbkpath = m_CSSUpdates.value(dest_oldbkpath,dest_oldbkpath);
                        if (!dest_newbkpath.isEmpty() && !m_newbookpath.isEmpty()) {
                            QString new_href = Utility::buildRelativePath(m_newbookpath, dest_newbkpath);
                            if (new_href.isEmpty()) new_href = destfile;
                            // Replace the old url with the new one
                            // But only replace if string has changed. Otherwise any matched
                            // quoted string content could potentially be unnecessarily url encoded.
                            // The hope is to only encode urls that were actually modified by renames.
                            if (new_href != frag_mo.captured(j)) {
                                new_href = Utility::URLEncodePath(new_href);
                                fragment.replace(frag_mo.capturedStart(j), frag_mo.capturedLength(j), new_href);
                                frag_index_correction += new_href.length() - frag_mo.capturedLength(j);
                                changes_made = true;
                            }
                        }
                    }
                    frag_start_index = frag_mo.capturedEnd() + frag_index_correction;
                    frag_index_correction = 0;
                    frag_mo = urls.match(fragment, frag_start_index);
                } while (frag_mo.hasMatch());
            }
            // Replace the original attribute string fragment with the new one
            if (changes_made) {
                result.replace(mo.capturedStart(i), mo.capturedLength(i), fragment);
                start_index_correction += fragment.length() - mo.capturedLength(i);
            }

        }
        start_index = mo.capturedEnd() + start_index_correction;
        start_index_correction = 0;
        mo = reference.match(result, start_index);
    } while (mo.hasMatch());

    return result;
}
