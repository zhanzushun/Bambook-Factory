#ifndef TXT_MAINWINDOW_H
#define TXT_MAINWINDOW_H

#include "ui_txtMainWindow.hpp"
#include "zconnect.h"
#include "zdisplay.h"
#include <QtCore/QThread>
#include <QtCore/QMutex>

class TxtConvertThread : public QThread
{
    Q_OBJECT

public:
    TxtConvertThread(QObject *parent = 0);
    ~TxtConvertThread();

    void start(const QString& txtFullPathFile);
    void stopProcess();
    bool* tobeAbort() {return &m_abort;}
    bool active() {return m_live;}
    void emitOnConverting(int phase, int progress, QString msg);

Q_SIGNALS:
    void onConverting(int phase, int progress, QString msg);

protected:
    void run();
private:
    QString m_txtFullPathFile;
    bool m_abort;
    QMutex m_mutexForAbort;
    bool m_live;
};

class TxtMainWindow : public QDialog, public Ui::TxtMainWindow
{
    Q_OBJECT
public:
    TxtMainWindow(QWidget *parent = 0);
    ~TxtMainWindow();

    void onDeviceConnected(const wchar_t* sn, const wchar_t* fv, int totalSpaceInKb, 
        int freeSpaceInKb, const list<ZDeviceBookInfo> &booklist);
    void onDeviceUpdated(int totalSpaceInKb, int freeSpaceInKb, 
        const list<ZDeviceBookInfo> &booklist);
    void onDeviceDisconnected();
    void onTransferingToDevice(int status, int progress, const wchar_t* snbFile, void* cbParam);
    TxtConvertThread* convertThread() {return m_convertThread;}

    public Q_SLOTS:
        void onSelectTxt();
        void onConvert();
        void onUpload();
        void onClose();
        void onRead();
        void onConverting(int phase, int progress, QString msg);

protected:
    virtual void closeEvent(QCloseEvent *);

private:
    void updateActions();
    bool copyToBB(const wstring& fileName, const wstring& bookName);
    bool runAsTxtFinal(); 

    bool m_connected;
    list<ZDeviceBookInfo> m_bambooklist;
    wstring m_snbFileName;
    wstring m_snbFileNameInProcess;
    TxtConvertThread* m_convertThread;
};

#endif
