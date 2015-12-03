#include "stdafx.h"
#include "txtMainWindow.hxx"
#include "zdisplay.h"
#include "chaptersEditor.hxx"
#include "zpdf.h"

static QWidget* g_mainwindowTxt = NULL;

void transferingToDeviceCB_Txt(int status, int progress, const wchar_t* snbFile, void* cbParam)
{
    Q_ASSERT(g_mainwindowTxt != NULL);
    if (g_mainwindowTxt == NULL)
        return;
    ((TxtMainWindow*)g_mainwindowTxt)->onTransferingToDevice(status, progress, snbFile, cbParam);
}

void onConnectedCB_Txt(const wchar_t* sn, const wchar_t* fv, int totalSpaceInKb, int freeSpaceInKb, 
    const list<ZDeviceBookInfo>& booklist)
{
    Q_ASSERT(g_mainwindowTxt != NULL);
    if (g_mainwindowTxt == NULL)
        return;
    ((TxtMainWindow*)g_mainwindowTxt)->onDeviceConnected(sn, fv, totalSpaceInKb, freeSpaceInKb, booklist);
}

void onBooklistChangedCB_Txt(int totalSpaceInKb, int freeSpaceInKb, 
    const list<ZDeviceBookInfo>& booklist)
{
    Q_ASSERT(g_mainwindowTxt != NULL);
    if (g_mainwindowTxt == NULL)
        return;
    ((TxtMainWindow*)g_mainwindowTxt)->onDeviceUpdated(totalSpaceInKb, freeSpaceInKb, booklist);
}

void onDisConnectedCB_Txt()
{
    Q_ASSERT(g_mainwindowTxt != NULL);
    if (g_mainwindowTxt == NULL)
        return;
    ((TxtMainWindow*)g_mainwindowTxt)->onDeviceDisconnected();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

inline QTextCodec* CODEC()
{
    QTextCodec* codec = QTextCodec::codecForName("GBK");
    if (codec == NULL)
        codec = QTextCodec::codecForLocale();
    Q_ASSERT(codec != NULL);
    return codec;
}

inline QString STR(const char* s)
{
    if (CODEC() == NULL)
        return s;
    return CODEC()->toUnicode(s);
}

TxtMainWindow::TxtMainWindow(QWidget *parent): QDialog(parent)
{
    setupUi(this);
    g_mainwindowTxt = this;
    if (runAsTxtFinal())
        g_mainwindow = g_mainwindowTxt;

    labelStatus->setTextFormat(Qt::RichText);
    if (runAsTxtFinal())
    {
        setWindowTitle(STR("文本剪刀终结者 - 版本0.3"));
    }
    else
    {
        labelStatus->setText("");
    }

    connect(btnSelectTxt, SIGNAL(clicked(bool)), this, SLOT(onSelectTxt()));
    connect(btnConvert, SIGNAL(clicked(bool)), this, SLOT(onConvert()));
    connect(btnUpload, SIGNAL(clicked(bool)), this, SLOT(onUpload()));
    connect(btnExit, SIGNAL(clicked(bool)), this, SLOT(onClose()));
    connect(btnRead, SIGNAL(clicked(bool)), this, SLOT(onRead()));

    m_connected = false;
    if (parent == NULL)
        ZStartConnectThread(onConnectedCB_Txt, onBooklistChangedCB_Txt, onDisConnectedCB_Txt);
    updateActions();

    m_convertThread = new TxtConvertThread(this);
    connect(m_convertThread, SIGNAL(onConverting(int, int, QString)), this, SLOT(onConverting(int, int, QString)));
}

TxtMainWindow::~TxtMainWindow()
{
    if (runAsTxtFinal())
        ZStopConnectThread(true);
    g_mainwindowTxt = NULL;
}

bool TxtMainWindow::runAsTxtFinal()
{
    return parent() == NULL;
}

void TxtMainWindow::onSelectTxt()
{
    editTxtFile->setText(QFileDialog::getOpenFileName(this,
        tr("Select Txt file"), "", tr("TXT File (*.txt)")));
    m_snbFileName = L"";
    updateActions();
}

inline wstring appPath()
{
    QString r = QCoreApplication::applicationDirPath();
    r += "/";
    r.replace('/','\\');
    wstring s;
    return qstr2wstr(s, r);
}

void TxtMainWindow::onConvert()
{
    if (m_convertThread->active())
    {
        bool bStop = false;
        QMessageBox msg;
        msg.setWindowTitle(tr("Stop converting"));
        msg.setText(tr("Stop converting previous TXT file"));
        QPushButton *YButton = msg.addButton(tr("OK"), QMessageBox::YesRole);
        QPushButton *NButton = msg.addButton(tr("Cancel"), QMessageBox::RejectRole);
        msg.exec();
        if (msg.clickedButton() == YButton)
            bStop = true;
        if (bStop)
            m_convertThread->stopProcess();
    }

    QString txtFullPathFile = editTxtFile->text();
    txtFullPathFile.replace('/', '\\');

    wstring snbFileName;
    {
        QFileInfo fi(txtFullPathFile);
        QString txtFileName = fi.fileName();
        QString ext = ".txt";
        QString txtFileNameWithoutExt = txtFileName.mid(0, txtFileName.length() - ext.length());
        qstr2wstr(snbFileName, txtFileNameWithoutExt);
        snbFileName += L".snb";
    }
    m_snbFileNameInProcess = snbFileName;
    m_convertThread->start(txtFullPathFile);
}

void TxtMainWindow::onUpload()
{
    if (m_snbFileName.empty())
        return;
    ZBookInfo bookInfo;
    if (!ZGetBookInfo(m_snbFileName.c_str(), bookInfo))
        return;
    copyToBB(m_snbFileName, bookInfo.name);
}

void TxtMainWindow::onClose()
{
    if (runAsTxtFinal())
    {
        ZStopConnectThread(true);
        QCoreApplication::quit();
    }
    else
    {
        done(0);
    }
}

void TxtMainWindow::onRead()
{
    ZReadBook(m_snbFileName.c_str(), true);
}

void TxtMainWindow::closeEvent(QCloseEvent *)
{
    onClose();
}

void TxtMainWindow::onDeviceConnected(const wchar_t* , const wchar_t* , int , 
    int , const list<ZDeviceBookInfo> &booklist)
{
    m_connected = true;
    m_bambooklist = booklist;
    updateActions();
}

void TxtMainWindow::onDeviceUpdated(int , int , 
    const list<ZDeviceBookInfo> &bl)
{
    m_bambooklist = bl;
}

void TxtMainWindow::onDeviceDisconnected()
{
    m_connected = false;
    updateActions();
}

void TxtMainWindow::updateActions()
{
    btnUpload->setEnabled(m_connected && !m_snbFileName.empty());
    btnRead->setEnabled(!m_snbFileName.empty());
    if (!runAsTxtFinal())
        btnUpload->setVisible(false);
}

bool TxtMainWindow::copyToBB(const wstring& fileName, const wstring& bookName)
{
    bool found = false;
    wstring replaceId;
    foreach (ZDeviceBookInfo rightbook, m_bambooklist)
    {
        if (fileName.compare(rightbook.id) == 0 || bookName.compare(rightbook.name) == 0)
        {
            found = true;
            replaceId = rightbook.id;
            break;
        }
    }
    bool bReplace = false;
    if (found)
    {
        QMessageBox msg;
        msg.setWindowTitle(tr("Replace?"));
        msg.setText(tr("Found similar book in Bambook"));
        QPushButton *replaceButton = msg.addButton(tr("Replace"), QMessageBox::YesRole);
        QPushButton *addNewButton = msg.addButton(tr("Add new"), QMessageBox::NoRole);
        QPushButton *cancelButton = msg.addButton(tr("Cancel"), QMessageBox::RejectRole);
        msg.exec();
        if (msg.clickedButton() == replaceButton)
            bReplace = true;
        else if (msg.clickedButton() == addNewButton)
            bReplace = false;
        else if (msg.clickedButton() == cancelButton)
            return true;
    }

    wstring file = ZGetSnbDir() + fileName;
    ZTransToDevice(file.c_str(), bReplace, replaceId.c_str(), transferingToDeviceCB_Txt, 0, true);
    return true;
}

void TxtMainWindow::onTransferingToDevice(int status, int progress, const wchar_t* snbFile, void*)
{
    if  (status == 1 || status == 2)
    {
        QFileInfo fi(wstr2qstr(snbFile));
        QString str = (status == 1) ? tr("Uploaded file %1 successfully.") : tr("Failed to upload file %");
        str = str.arg(fi.fileName());
        QString labelStr = str;
        
        if (runAsTxtFinal())
            labelStr += tr(", you can use <a href='http://bbsdk.sdo.com/opus_detail.do?sid=9f88823d17ba8bdfee025fba88cf5e15'>'Bambook Factory'</a> to manage your SNB books");

        labelStatus->setTextFormat(Qt::RichText);
        labelStatus->setText(labelStr);
        labelStatus->setOpenExternalLinks(true);

        QMessageBox msg;
        msg.setText(str);
        msg.setWindowTitle(tr("Translation completed"));
        msg.exec();
        return;
    }
    Q_ASSERT(status == 0);
    labelStatus->setText(tr("Translation progress %1%").arg(progress));
}

//////////////////////////////////////////////////////////////////////////////////////////////////

TxtConvertThread::TxtConvertThread(QObject *parent) : QThread(parent)
{
    m_live = false;
    m_abort = false;
}

TxtConvertThread::~TxtConvertThread()
{
    stopProcess();
}

// run in main thread.
void TxtConvertThread::stopProcess()
{
    m_mutexForAbort.lock();
    m_abort = true;
    m_mutexForAbort.unlock();
    quit(); // quit the eventloop
    wait(); // wait until run() exited.
    m_abort = false;
}

void TxtConvertThread::start(const QString& txtFullPathFile)
{
    m_txtFullPathFile = txtFullPathFile;
    QThread::start();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

// run in convert thread

#pragma comment(lib, "..\\bin\\PdfExporter.lib")

typedef void(* ZOnConvertingCallack)(int phase, int progress, QString msg);

extern "C" _declspec(dllimport) 
    bool ZConvertPdf(wchar_t* snbRootDir, int bufLen, const wchar_t* xpdfPath0, 
    const wchar_t* pdfFile, const wchar_t* userDir0, bool isColumnMode, bool extractImages,
    int chapterType, bool* abort, ZOnConvertingCallack cb);

void onConvertingCallack2(int phase, int progress, QString msg)
{
    TxtMainWindow* p = (TxtMainWindow*) g_mainwindowTxt;
    if (p == NULL)
        return;
    if (p->convertThread() == NULL)
        return;
    p->convertThread()->emitOnConverting(phase, progress, msg);
}

void TxtConvertThread::emitOnConverting(int phase, int progress, QString msg)
{
    emit onConverting(phase, progress, msg);
}

void TxtConvertThread::run()
{
    m_live = true;
    wchar_t txt2SnbRootDir[1024];
    {
        wstring xpdfPath = appPath() + L"xpdf\\";
        wstring txtFile; 
        qstr2wstr(txtFile, m_txtFullPathFile);
        if (!ZConvertTxt(txt2SnbRootDir, 1024, txtFile.c_str(), ZGetHomeDir(), 
            &m_abort, onConvertingCallack2))
        {
            emit onConverting(-1, 0, "");
            m_live = false;
            return;
        }
    }

    wstring snbFileName;
    {
        QFileInfo fi(m_txtFullPathFile);
        QString txtFileName = fi.fileName();
        QString ext = ".txt";
        QString txtFileNameWithoutExt = txtFileName.mid(0, txtFileName.length() - ext.length());
        qstr2wstr(snbFileName, txtFileNameWithoutExt);
        snbFileName += L".snb";
    }

    wstring snb = wstring(ZGetSnbDir()) + snbFileName;
    if (!packSnb(snb.c_str(), txt2SnbRootDir))
    {
        emit onConverting(-1, 0, "");
        m_live = false;
        return;
    }
    emit onConverting(0, 0, "");
    m_live = false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

// run in main thread

void TxtMainWindow::onConverting(int phase, int progress, QString msg)
{
    if (phase == PHASE_AllDone)
    {
        m_snbFileName = m_snbFileNameInProcess;
        updateActions();
        labelStatus->setText(msg + tr("Imported file successfully, saving as %1%2").arg(wstr2qstr(ZGetSnbDir()))
            .arg(wstr2qstr(m_snbFileName.c_str())));

        QMessageBox msg;
        msg.setWindowTitle(tr("Done"));
        msg.setText(tr("Imported file successfully, do you want to read it?"));
        QPushButton *YButton = msg.addButton(tr("Read"), QMessageBox::YesRole);
        QPushButton *NButton = msg.addButton(tr("Cancel"), QMessageBox::ActionRole);
        QPushButton *BButton = NULL;
        if (!runAsTxtFinal())
            BButton = msg.addButton(tr("Back to the main window"), QMessageBox::RejectRole);
        msg.exec();
        if (msg.clickedButton() == YButton)
        {
            wstring f = m_snbFileName;
            if (!runAsTxtFinal())
                onClose();
            ZReadBook(f.c_str(), true);
        }
        else if (msg.clickedButton() == BButton)
        {
            onClose();
        }

    }
    else if (phase == PHASE_ERROR)
    {
        QMessageBox mb;
        mb.setWindowTitle(tr("Failed"));
        mb.setText(tr("Failed to import file."));
        mb.exec();
        labelStatus->setText(tr("Failed to import file."));
    }
    else if (phase == PHASE_7_ChooseChapters)
    {
        QVector<ChapterInfo>* pChapters1 = NULL;
        QVector<ChapterInfo>* pChapters2 = NULL;
        QMutex* pMutex = NULL;
        bool* pUseLeft = NULL;
        bool* pRetry = NULL;
        ZGetChapters(pChapters1, pChapters2, pMutex, pUseLeft, pRetry);

        pMutex->lock();
        ChaptersEditor editor(pChapters1, NULL, this);
        if (*pRetry == true)
            editor.buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
        else
            editor.buttonBox->button(QDialogButtonBox::Retry)->setText(tr("Extend the terms and retry"));

        editor.buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Ok"));
        editor.buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Use one chapter for whole book."));

        if (editor.exec() == QDialog::Accepted)
        {
            (*pUseLeft) = (editor.useLeft());
            (*pRetry) = (editor.retry());
        }
        else
        {
            QMessageBox mb;
            mb.setWindowTitle(tr("Cancelled"));
            mb.setText(tr("Use one chapter for whole book."));
            mb.exec();
            ChapterInfo a = pChapters1->at(0);
            pChapters1->clear();
            pChapters1->push_back(a);
        }
        pMutex->unlock();
    }
    else
        labelStatus->setText(msg);
}
