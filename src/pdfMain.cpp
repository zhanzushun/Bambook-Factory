#include "stdafx.h"
#include "pdfMainwindow.hxx"
//#include <QtWidgets/QPlastiqueStyle>

QWidget* g_mainwindow = NULL;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    //QApplication::setStyle(new QPlastiqueStyle);

    QTranslator translater;
    translater.load("bf_zh", QCoreApplication::applicationDirPath());
    app.installTranslator(&translater);

    PdfMainWindow window;

    {
        QProcess* p = new QProcess(&window);
        QString s = "regapp.exe da7e2090bfa6314781364e2c2baddf9e \"";
        s += QCoreApplication::applicationFilePath();
        s += "\"";
        s.replace('/','\\');
        p->start(s);
    }

    window.show();

    return app.exec();
}
