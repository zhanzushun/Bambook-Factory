#ifndef STDAFX_H
#define STDAFX_H

#include <QtCore/QtCore>
#include <QtGui/QtGui>
#include <QtXml/QtXml>

#include <string>
#include <set>
#include <list>
#include <map>

using namespace std;

void str2wstr(wstring & wstr, const char* str);
const char* wstr2str(QByteArray & buf, const wchar_t* wstr);
QString wstr2qstr(const wchar_t* wstr);
wstring& qstr2wstr(wstring& wstr, const QString &string);

bool packSnb(const wchar_t * snbName, const wchar_t * rootDir);
bool unpackSnb(const wchar_t * snbName, const wchar_t * relativePath, const wchar_t * outputName);
bool verifySnb(const wchar_t * snbName);

class QWidget;
extern QWidget* g_mainwindow;

#endif