#-------------------------------------------------
#
# Project created by QtCreator 2015-12-04T20:48:42
#
#-------------------------------------------------

QT       += core gui widgets

TARGET = pdfExporter
TEMPLATE = lib

DEFINES += PDFEXPORTER_LIBRARY

SOURCES += PdfExporter.cpp

HEADERS += stdafx.h zpdf.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}
