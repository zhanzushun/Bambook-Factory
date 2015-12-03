#ifndef READERWINDOW_H
#define READERWINDOW_H

#include "ui_readerWindow.hpp"
#include <QtGui/QMainWindow>
#include <QtCore/QTextStream>
class QFile;

class ReaderWindow : public QMainWindow, public Ui::ReaderWindow
{
    Q_OBJECT
public:
    ReaderWindow(QWidget *parent = 0);
    ~ReaderWindow();
    bool readBook(const wchar_t* snb, bool forceReflash);
public Q_SLOTS:
    void leftAnchorClicked(const QUrl&);
    void rightAnchorClicked(const QUrl&);
private:
    void readPage();

    QString m_snbbook;
    QTextStream* m_pChapterStream;
    QFile* m_pChapterFile;
    int m_currentHeight;
    bool m_enterTextBlock;
    bool m_endTextBlock;
};

#endif
