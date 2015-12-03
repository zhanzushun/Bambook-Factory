#include "stdafx.h"
#include "txtMainwindow.hxx"
#include <QtGui/QPlastiqueStyle>

QWidget* g_mainwindow = NULL;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    //QApplication::setStyle(new QPlastiqueStyle);

    QTranslator translater;
    translater.load("bf_zh", QCoreApplication::applicationDirPath());
    app.installTranslator(&translater);

    TxtMainWindow window;

    {
        QProcess* p = new QProcess(&window);
        QString s = "regapp.exe e83dc58b4ae9a3943906be528df333d8 \"";
        s += QCoreApplication::applicationFilePath();
        s += "\"";
        s.replace('/','\\');
        p->start(s);
    }

    window.show();

    return app.exec();
}
