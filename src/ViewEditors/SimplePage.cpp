/************************************************************************
**
**  Copyright (C) 2019-2024 Kevin B. Hendricks, Stratford Ontario Canada
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
#include <QUrl>
#include <QWebEngineProfile>
#include <QDebug>
#include "Misc/Utility.h"
#include "ViewEditors/SimplePage.h"

 
SimplePage::SimplePage(QWebEngineProfile* profile, QObject *parent)
    : QWebEnginePage(profile, parent)
{
    setBackgroundColor(Utility::WebViewBackgroundColor(true));
    setUrl(QUrl("about:blank"));
}

