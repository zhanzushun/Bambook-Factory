#include "stdafx.h"
#include "pdfMainWindow.hxx"
#include "zdisplay.h"
#include "chaptersEditor.hxx"
#include "zpdf.h"

static QWidget* g_mainwindowPdf = NULL;

void transferingToDeviceCB_Pdf(int status, int progress, const wchar_t* snbFile, void* cbParam)
{
    Q_ASSERT(g_mainwindowPdf != NULL);
    if (g_mainwindowPdf == NULL)
        return;
    ((PdfMainWindow*)g_mainwindowPdf)->onTransferingToDevice(status, progress, snbFile, cbParam);
}

void onConnectedCB_Pdf(const wchar_t* sn, const wchar_t* fv, int totalSpaceInKb, int freeSpaceInKb, 
    const list<ZDeviceBookInfo>& booklist)
{
    Q_ASSERT(g_mainwindowPdf != NULL);
    if (g_mainwindowPdf == NULL)
        return;
    ((PdfMainWindow*)g_mainwindowPdf)->onDeviceConnected(sn, fv, totalSpaceInKb, freeSpaceInKb, booklist);
}

void onBooklistChangedCB_Pdf(int totalSpaceInKb, int freeSpaceInKb, 
    const list<ZDeviceBookInfo>& booklist)
{
    Q_ASSERT(g_mainwindowPdf != NULL);
    if (g_mainwindowPdf == NULL)
        return;
    ((PdfMainWindow*)g_mainwindowPdf)->onDeviceUpdated(totalSpaceInKb, freeSpaceInKb, booklist);
}

void onDisConnectedCB_Pdf()
{
    Q_ASSERT(g_mainwindowPdf != NULL);
    if (g_mainwindowPdf == NULL)
        return;
    ((PdfMainWindow*)g_mainwindowPdf)->onDeviceDisconnected();
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

PdfMainWindow::PdfMainWindow(QWidget *parent): QDialog(parent)
{
    setupUi(this);
    g_mainwindowPdf = this;
    if (runAsPdfFinal())
        g_mainwindow = g_mainwindowPdf;
    
    labelStatus->setTextFormat(Qt::RichText);
    if (runAsPdfFinal())
    {
        setWindowTitle(STR("PDF ÖÕ½áÕß - °æ±¾0.3.1"));
    }
    else
    {
        labelStatus->setText("");
    }

    connect(btnSelectPdf, SIGNAL(clicked(bool)), this, SLOT(onSelectPdf()));
    connect(btnConvert, SIGNAL(clicked(bool)), this, SLOT(onConvert()));
    connect(btnUpload, SIGNAL(clicked(bool)), this, SLOT(onUpload()));
    connect(btnExit, SIGNAL(clicked(bool)), this, SLOT(onClose()));
    connect(btnRead, SIGNAL(clicked(bool)), this, SLOT(onRead()));

    m_connected = false;
    if (parent == NULL)
        ZStartConnectThread(onConnectedCB_Pdf, onBooklistChangedCB_Pdf, onDisConnectedCB_Pdf);
    updateActions();

    m_convertThread = new ConvertThread(this);
    connect(m_convertThread, SIGNAL(onConverting(int, int, QString)), this, SLOT(onConverting(int, int, QString)));
}

PdfMainWindow::~PdfMainWindow()
{
    if (runAsPdfFinal())
        ZStopConnectThread(true);
    g_mainwindowPdf = NULL;
}

bool PdfMainWindow::runAsPdfFinal()
{
    return parent() == NULL;
}

void PdfMainWindow::onSelectPdf()
{
    editPdfFile->setText(QFileDialog::getOpenFileName(this,
        tr("Select PDF file"), "", tr("PDF File (*.pdf)")));
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

void PdfMainWindow::onConvert()
{
    if (m_convertThread->active())
    {
        bool bStop = false;
        QMessageBox msg;
        msg.setWindowTitle(tr("Stop converting"));
        msg.setText(tr("Stop converting previous PDF file"));
        QPushButton *YButton = msg.addButton(tr("OK"), QMessageBox::YesRole);
        QPushButton *NButton = msg.addButton(tr("Cancel"), QMessageBox::RejectRole);
        msg.exec();
        if (msg.clickedButton() == YButton)
            bStop = true;
        if (bStop)
            m_convertThread->stopProcess();
    }

    bool isMultiColumn = !rdSingleColumn->isChecked();
    bool isTextOnly = rdTextOnly->isChecked();
    QString pdfFullPathFile = editPdfFile->text();
    pdfFullPathFile.replace('/', '\\');
    int chapterType = (rdOneChapter->isChecked()) ? 1 : (rdChapterPerPage->isChecked() ? 2 : 3);
    m_chapterType = chapterType;

    wstring snbFileName;
    {
        QFileInfo fi(pdfFullPathFile);
        QString pdfFileName = fi.fileName();
        QString ext = ".pdf";
        QString pdfFileNameWithoutExt = pdfFileName.mid(0, pdfFileName.length() - ext.length());
        qstr2wstr(snbFileName, pdfFileNameWithoutExt);
        snbFileName += L".snb";
    }
    m_snbFileNameInProcess = snbFileName;
    m_convertThread->start(pdfFullPathFile, isMultiColumn, !isTextOnly, chapterType);
}

void PdfMainWindow::onUpload()
{
    if (m_snbFileName.empty())
        return;
    ZBookInfo bookInfo;
    if (!ZGetBookInfo(m_snbFileName.c_str(), bookInfo))
        return;
    copyToBB(m_snbFileName, bookInfo.name);
}

void PdfMainWindow::onClose()
{
    if (runAsPdfFinal())
    {
        ZStopConnectThread(true);
        QCoreApplication::quit();
    }
    else
    {
        done(0);
    }
}

void PdfMainWindow::onRead()
{
    ZReadBook(m_snbFileName.c_str(), true);
}

void PdfMainWindow::closeEvent(QCloseEvent *)
{
    onClose();
}

void PdfMainWindow::onDeviceConnected(const wchar_t* , const wchar_t* , int , 
    int , const list<ZDeviceBookInfo> &booklist)
{
    m_connected = true;
    m_bambooklist = booklist;
    updateActions();
}

void PdfMainWindow::onDeviceUpdated(int , int , 
    const list<ZDeviceBookInfo> &bl)
{
    m_bambooklist = bl;
}

void PdfMainWindow::onDeviceDisconnected()
{
    m_connected = false;
    updateActions();
}

void PdfMainWindow::updateActions()
{
    btnUpload->setEnabled(m_connected && !m_snbFileName.empty());
    btnRead->setEnabled(!m_snbFileName.empty());
    if (!runAsPdfFinal())
        btnUpload->setVisible(false);
}

bool PdfMainWindow::copyToBB(const wstring& fileName, const wstring& bookName)
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
    ZTransToDevice(file.c_str(), bReplace, replaceId.c_str(), transferingToDeviceCB_Pdf, 0, true);
    return true;
}

void PdfMainWindow::onTransferingToDevice(int status, int progress, const wchar_t* snbFile, void*)
{
    if  (status == 1 || status == 2)
    {
        QFileInfo fi(wstr2qstr(snbFile));
        QString str = (status == 1) ? tr("Uploaded file %1 successfully.") : tr("Failed to upload file %");
        str = str.arg(fi.fileName());
        QString labelStr = str;
        
        if (runAsPdfFinal())
            labelStr += tr(", you can use <a href='http://bbsdk.sdo.com/opus_detail.do?sid=9f88823d17ba8bdfee025fba88cf5e15'>'Bambook Factory'</a> to manage your SNB books");

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

ConvertThread::ConvertThread(QObject *parent) : QThread(parent)
{
    m_live = false;
    m_abort = false;
}

ConvertThread::~ConvertThread()
{
    stopProcess();
}

// run in main thread.
void ConvertThread::stopProcess()
{
    m_mutexForAbort.lock();
    m_abort = true;
    m_mutexForAbort.unlock();
    quit(); // quit the eventloop
    wait(); // wait until run() exited.
    m_abort = false;
}

void ConvertThread::start(const QString& pdfFullPathFile, bool isMultiColumn, bool doImage, 
    int chapterType)
{
    m_pdfFullPathFile = pdfFullPathFile;
    m_isMultiColumn = isMultiColumn;
    m_doImage = doImage;
    m_chapterType = chapterType;
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

void onConvertingCallack(int phase, int progress, QString msg)
{
    PdfMainWindow* p = (PdfMainWindow*) g_mainwindowPdf;
    if (p == NULL)
        return;
    if (p->convertThread() == NULL)
        return;
    p->convertThread()->emitOnConverting(phase, progress, msg);
}

void ConvertThread::emitOnConverting(int phase, int progress, QString msg)
{
    emit onConverting(phase, progress, msg);
}

void ConvertThread::run()
{
    m_live = true;
    wchar_t pdf2SnbRootDir[1024];
    {
        wstring xpdfPath = appPath() + L"xpdf\\";
        wstring pdfFile; 
        qstr2wstr(pdfFile, m_pdfFullPathFile);
        if (!ZConvertPdf(pdf2SnbRootDir, 1024, xpdfPath.c_str(), pdfFile.c_str(), ZGetHomeDir(), 
            m_isMultiColumn, m_doImage, m_chapterType, &m_abort, onConvertingCallack))
        {
            emit onConverting(-1, 0, "");
            m_live = false;
            return;
        }
    }

    wstring snbFileName;
    {
        QFileInfo fi(m_pdfFullPathFile);
        QString pdfFileName = fi.fileName();
        QString ext = ".pdf";
        QString pdfFileNameWithoutExt = pdfFileName.mid(0, pdfFileName.length() - ext.length());
        qstr2wstr(snbFileName, pdfFileNameWithoutExt);
        snbFileName += L".snb";
    }

    wstring snb = wstring(ZGetSnbDir()) + snbFileName;
    if (!packSnb(snb.c_str(), pdf2SnbRootDir))
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

void PdfMainWindow::onConverting(int phase, int progress, QString msg)
{
    if (phase == PHASE_AllDone)
    {
        m_snbFileName = m_snbFileNameInProcess;
        updateActions();
        labelStatus->setText(msg + tr("Imported PDF file successfully, saving as %1%2").arg(wstr2qstr(ZGetSnbDir()))
            .arg(wstr2qstr(m_snbFileName.c_str())));

        QMessageBox msg;
        msg.setWindowTitle(tr("Done"));
        msg.setText(tr("Imported PDF file successfully, do you want to read it?"));
        QPushButton *YButton = msg.addButton(tr("Read"), QMessageBox::YesRole);
        QPushButton *NButton = msg.addButton(tr("Cancel"), QMessageBox::ActionRole);
        QPushButton *BButton = NULL;
        if (!runAsPdfFinal())
            BButton = msg.addButton(tr("Back to the main window"), QMessageBox::RejectRole);
        msg.exec();
        if (msg.clickedButton() == YButton)
        {
            wstring f = m_snbFileName;
            if (!runAsPdfFinal())
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
        mb.setText(tr("Failed to import PDF file."));
        mb.exec();
        labelStatus->setText(tr("Failed to import PDF file."));
    }
    else if (phase == PHASE_7_ChooseChapters)
    {
        QVector<ChapterInfo>* pChapters1 = NULL;
        QVector<ChapterInfo>* pChapters2 = NULL;
        QMutex* pMutex = NULL;
        bool* pUseLeft = NULL;
        bool* pRetry_NotUsed = NULL;
        ZGetChapters(pChapters1, pChapters2, pMutex, pUseLeft, pRetry_NotUsed);

        if (m_chapterType == 3)
        {
            pMutex->lock();
            ChaptersEditor editor(pChapters1, pChapters2, this);
            editor.buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

            if (editor.exec() == QDialog::Accepted)
            {
                (*pUseLeft) = (editor.useLeft());
            }
            else
            {
                QMessageBox mb;
                mb.setWindowTitle(tr("Cancelled"));
                mb.setText(tr("Use default chapters"));
                mb.exec();
            }
            pMutex->unlock();
        }
    }
    else
        labelStatus->setText(msg);
}
