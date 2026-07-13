#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>
#include "CSSParser.h"


QString ReadUnicodeTextFile(const QString &fullfilepath)
{
  QFile file(fullfilepath);
  if (!file.open(QFile::ReadOnly)) {
    QString msg = fullfilepath + ": " + file.errorString();
    qDebug() << msg;
  }
  QTextStream in(&file);
  in.setAutoDetectUnicode(true);
  return in.readAll();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QStringList arguments = QCoreApplication::arguments();
    QString filepath;
    if (arguments.size() > 1) {
       filepath = arguments.at(1);
    }
    QString css_src;
    if (!filepath.isEmpty()) {
        css_src = ReadUnicodeTextFile(filepath);
    }
    if (!css_src.isEmpty()) {
        CSSParser rrrr;
        rrrr.parse_css(css_src);
    }
    return 0;
}

