/************************************************************************
**
**  Copyright (C) 2009, 2010  Nokia Corporation, Strahinja Markovic
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

#ifndef CSSHIGHLIGHTER_H
#define CSSHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include "Misc/SettingsStore.h"

class CSSHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:

    explicit CSSHighlighter(QObject *parent);

    void do_rehighlight();

protected:

    void highlightBlock(const QString &text);
    void highlight(const QString &, int start, int length, int state);

private:

    SettingsStore::CodeViewAppearance m_codeViewAppearance;
};

#endif // CSSHIGHLIGHTER_H
