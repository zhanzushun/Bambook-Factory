#include "stdafx.h"
#include "mainwindow.hxx"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath());

    MainWindow window;

    {
        QProcess* p = new QProcess(g_mainwindow);
        QString s = "regapp.exe 9f88823d17ba8bdfee025fba88cf5e15 \"";
        s += QCoreApplication::applicationFilePath();
        s += "\"";
        s.replace('/','\\');
        p->start(s);
    }

    window.showMaximized();

    return app.exec();
}
