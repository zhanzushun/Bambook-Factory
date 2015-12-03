#ifndef PDFMAINWINDOW_H
#define PDFMAINWINDOW_H

#include "ui_pdfMainWindow.hpp"
#include "zconnect.h"
#include "zdisplay.h"
#include <QtCore/QThread>
#include <QtCore/QMutex>

class ConvertThread : public QThread
{
    Q_OBJECT

public:
    ConvertThread(QObject *parent = 0);
    ~ConvertThread();

    void start(const QString& pdfFullPathFile, bool isMultiColumn, bool doImage, int chapterType);
    void stopProcess();
    bool* tobeAbort() {return &m_abort;}
    bool active() {return m_live;}
    void emitOnConverting(int phase, int progress, QString msg);

Q_SIGNALS:
    void onConverting(int phase, int progress, QString msg);

protected:
    void run();
private:
    QString m_pdfFullPathFile;
    bool m_isMultiColumn;
    bool m_doImage;
    int m_chapterType;
    bool m_abort;
    QMutex m_mutexForAbort;
    bool m_live;
};

class PdfMainWindow : public QDialog, public Ui::PdfMainWindow
{
    Q_OBJECT
public:
    PdfMainWindow(QWidget *parent = 0);
    ~PdfMainWindow();

    void onDeviceConnected(const wchar_t* sn, const wchar_t* fv, int totalSpaceInKb, 
        int freeSpaceInKb, const list<ZDeviceBookInfo> &booklist);
    void onDeviceUpdated(int totalSpaceInKb, int freeSpaceInKb, 
        const list<ZDeviceBookInfo> &booklist);
    void onDeviceDisconnected();
    void onTransferingToDevice(int status, int progress, const wchar_t* snbFile, void* cbParam);
    ConvertThread* convertThread() {return m_convertThread;}

    public Q_SLOTS:
        void onSelectPdf();
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
    bool runAsPdfFinal(); 

    bool m_connected;
    list<ZDeviceBookInfo> m_bambooklist;
    wstring m_snbFileName;
    wstring m_snbFileNameInProcess;
    ConvertThread* m_convertThread;
    int m_chapterType;
};

#endif
