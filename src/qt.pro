#-------------------------------------------------
#
# Project created by QtCreator 2015-12-04T19:33:12
#
#-------------------------------------------------

QT       += core gui widgets

TARGET = qt
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    chaptersEditor.cpp \
    connectThread.cpp \
    pdfMainWindow.cpp \
    readerWindow.cpp \
    stdafx.cpp \
    txtMainWindow.cpp

HEADERS  += mainwindow.hxx \
    chaptersEditor.hxx \
    connectThread.hxx \
    pdfMainWindow.hxx \
    readerWindow.hxx \
    stdafx.h \
    txtMainWindow.hxx \
    zconnect.h \
    zdisplay.h \
    zpdf.h

FORMS    += mainwindow.ui \
    chaptersEditor.ui \
    pdfMainWindow.ui \
    readerWindow.ui \
    txtMainWindow.ui

RESOURCES += \
    bf.qrc

DISTFILES += \
    lrelease.bat \
    lupdate.bat \
    bf_en.ts \
    bf_zh.ts

INCLUDEPATH += ../third-party/include
LIBPATH     += ../third-party/lib
LIBS        += -lBambookCore

QMAKE_CXXFLAGS += -m32
