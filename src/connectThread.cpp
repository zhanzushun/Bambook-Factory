#include "stdafx.h"
#include "..\third-party\include\BambookCore.h"
#include "connectthread.hxx"

//#pragma comment(lib, "..\\third-party\\lib\\BambookCore.lib")

static ConnectThread* g_connectThread = NULL;
static ConnectThreadHost* g_connectThreadHost = NULL;
static ZOnConnectedCB g_onConnectCB = NULL;
static ZOnBooklistChangedCB g_onBookListChangedCB = NULL;
static ZOnDisConnectedCB g_onDisConnectCB = NULL;
static ZTranFromDeviceCB g_transferFromDeviceCB = NULL;
static ZTransToDeviceCB g_transferToDeviceCB = NULL;

inline QTextCodec* CODEC()
{
    QTextCodec* codec = QTextCodec::codecForName("GBK");
    if (codec == NULL)
        codec = QTextCodec::codecForLocale();
    Q_ASSERT(codec != NULL);
    return codec;
}

void str2wstr(wstring & wstr, const char* str)
{
    QByteArray encodedString = str;
    QString string = CODEC()->toUnicode(encodedString);
    wstr = qstr2wstr(wstr, string);
}

const char* wstr2str(QByteArray & buf, const wchar_t* wstr)
{
    buf = CODEC()->fromUnicode(wstr2qstr(wstr));
    return buf.constData();
}

QString wstr2qstr(const wchar_t* wstr)
{
    if (sizeof(wchar_t) == sizeof(unsigned short))
        return QString::fromUtf16((const ushort *)wstr);
    else
        return QString::fromUcs4((const uint *)wstr);
}

wstring& qstr2wstr(wstring& wstr, const QString &string) 
{
    if (sizeof(wchar_t) == sizeof(unsigned short))
        wstr = (const wchar_t*) string.unicode();
    else
        Q_ASSERT(false);
    return wstr;
}

struct CallbackUserData
{
    wstring book;
    bool fromDevice;
    bool notifyBooklistChanged;
    void* oldData;
};

void gTransCallback(uint32_t status, uint32_t progress, intptr_t2 userData)
{
    if (g_connectThreadHost == NULL)
        return;

    BambookTransferCallbackSender sender;
    QObject::connect(&sender, SIGNAL(onTransfering(int, int, void*)), g_connectThreadHost,
        SLOT(onTransfering(int,int,void*)));
    sender.emitTransfering((int)status, (int)progress, (void*)userData);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

ConnectThreadHost::ConnectThreadHost(QObject *parent): QObject(parent)
{

}

void ConnectThreadHost::onConnected()
{
    if (g_connectThread != NULL)
    {
        g_connectThread->deviceMutex().lock();
        DeviceData devData = g_connectThread->deviceData();
        g_connectThread->deviceMutex().unlock();
        if (g_onConnectCB != NULL)
        {
            g_onConnectCB(devData.sn(), devData.fv(), devData.totalSpaceInKb(),
                devData.freeSpaceInKb(), devData.bookList());
        }
    }
}

void ConnectThreadHost::onBookListChanged()
{
    if (g_connectThread != NULL)
    {
        g_connectThread->deviceMutex().lock();
        DeviceData devData = g_connectThread->deviceData();
        g_connectThread->deviceMutex().unlock();
        if (g_onBookListChangedCB != NULL)
        {
            g_onBookListChangedCB(devData.totalSpaceInKb(),
                devData.freeSpaceInKb(), devData.bookList());
        }
    }
}

void ConnectThreadHost::onDisConnected()
{
    if (g_onDisConnectCB != NULL)
    {
        g_onDisConnectCB();
    }
}

void ConnectThreadHost::init()
{
    connect(g_connectThread, SIGNAL(onConnected()), this, SLOT(onConnected()));
    connect(g_connectThread, SIGNAL(onBookListChanged()), this, SLOT(onBookListChanged()));
    connect(g_connectThread, SIGNAL(onDisConnected()), this, SLOT(onDisConnected()));
}

void ConnectThreadHost::onTransfering(int status, int progress, void* userData)
{
    CallbackUserData* data = (CallbackUserData*)userData;
    if (data != NULL)
    {
        if (data->fromDevice && g_transferFromDeviceCB != NULL)
            g_transferFromDeviceCB((int)status, (int)progress, data->book.c_str(), data->oldData);
        else
            g_transferToDeviceCB((int)status, (int)progress, data->book.c_str(), data->oldData);
        if (!data->fromDevice && data->notifyBooklistChanged && status == TRANS_STATUS_DONE)
            g_connectThread->setBookListChanged();
        if (status == TRANS_STATUS_DONE || status == TRANS_STATUS_ERR)
            delete data;
    }
}

void BambookTransferCallbackSender::emitTransfering(int status, int progress, void* userData)
{
    emit onTransfering(status, progress, userData);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

bool ZStartConnectThread(ZOnConnectedCB cb1, ZOnBooklistChangedCB cb2, ZOnDisConnectedCB cb3)
{
    if (g_connectThread != NULL)
        return false;
    g_connectThread = new ConnectThread();
    g_connectThreadHost = new ConnectThreadHost();
    g_connectThreadHost->init();
    g_onConnectCB = cb1;
    g_onBookListChangedCB = cb2;
    g_onDisConnectCB = cb3;
    g_connectThread->start();
    return true;
}

bool ZTransFromDevice( const wchar_t* bookId, const wchar_t* snbPath, ZTranFromDeviceCB callback, 
    void* cbParam)
{
    g_transferFromDeviceCB = callback;
    g_connectThread->transferFromDevice(bookId, snbPath, cbParam);
    return true;
}

bool ZTransToDevice(const wchar_t* snbFile, bool replace, const wchar_t* replacedBookId, 
    ZTransToDeviceCB callback, void* cbParam, bool notifyBooklistChanged)
{
    g_transferToDeviceCB = callback;
    g_connectThread->transferToDevice(snbFile, replace, replacedBookId, cbParam, 
        notifyBooklistChanged);
    return true;
}

bool ZDeleteDeviceBook(const wchar_t* bookId, bool notifyBooklistChanged)
{
    g_connectThread->deleteDeviceBook(bookId, notifyBooklistChanged);
    return true;
}

bool ZStopConnectThread(bool bForce)
{
    if (g_connectThread)
    {
        if (bForce)
        {
            g_connectThread->terminate();
            g_connectThread->wait();
        }
        else
            g_connectThread->stopProcess();
        delete g_connectThread;
        g_connectThread = NULL;
    }
    if (g_connectThreadHost)
    {
        delete g_connectThreadHost;
        g_connectThreadHost = NULL;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

DeviceData::DeviceData()
{
    m_totalSpaceInKb = 0;
    m_freeSpaceInKb = 0;
}

DeviceData::DeviceData(const DeviceData &other)
{
    if (&other == this)
        return;
    m_sn = other.sn();
    m_fv = other.fv();
    m_totalSpaceInKb = other.totalSpaceInKb();
    m_freeSpaceInKb = other.freeSpaceInKb();
    m_booklist = other.bookList();
}

DeviceData::~DeviceData()
{

}

void DeviceData::init(const wchar_t* sn, const wchar_t* fv, int ts, int fs, 
    const list<ZDeviceBookInfo> & bookList)
{
    m_sn = sn;
    m_fv = fv;
    m_totalSpaceInKb = ts;
    m_freeSpaceInKb = fs;
    m_booklist = bookList;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

// run in main thread.
ConnectThread::ConnectThread(QObject *parent)
    : QThread(parent)
{
    m_abort = false;
    m_bookListChanged = false;
}

// run in main thread.
ConnectThread::~ConnectThread()
{
    stopProcess();
}

// run in main thread.
void ConnectThread::stopProcess()
{
    m_mutexForAbort.lock();
    m_abort = true;
    m_mutexForAbort.unlock();
    quit(); // quit the eventloop
    wait(); // wait until run() exited.
}

static BB_HANDLE g_bbHandle = NULL;

// run in connect thread
void addBookToList(list<ZDeviceBookInfo> & booklist, const PrivBookInfo & pbi)
{
    ZDeviceBookInfo zpbi;
    str2wstr(zpbi.id, pbi.bookGuid);
    str2wstr(zpbi.name, pbi.bookName);
    str2wstr(zpbi.author, pbi.bookAuthor);
    str2wstr(zpbi.abstract, pbi.bookAbstract);
    booklist.push_back(zpbi);
}

// run in connect thread
bool ConnectThread::getDeviceInfo(DeviceInfo& di, list<ZDeviceBookInfo>& booklist) 
{
    Q_ASSERT(g_bbHandle != NULL);
    di.cbSize = sizeof(DeviceInfo);
    if (BR_SUCC == BambookGetDeviceInfo(g_bbHandle, &di))
    {
        booklist.clear();
        PrivBookInfo pbi;
        pbi.cbSize = sizeof(PrivBookInfo);
        if (BR_SUCC == BambookGetFirstPrivBookInfo(g_bbHandle, &pbi))
        {
            addBookToList(booklist, pbi);
            while (BR_SUCC == BambookGetNextPrivBookInfo(g_bbHandle, &pbi))
                addBookToList(booklist, pbi);
        }
        return true;
    }
    return false;
}

// run in connect thread
void ConnectThread::fillDeviceData(const DeviceInfo& di, const list<ZDeviceBookInfo>& booklist)
{
    wstring sn, fv;
    str2wstr(sn, di.sn);
    str2wstr(fv, di.firmwareVersion);

    m_mutexForDeviceData.lock();
    m_deviceData.init(sn.c_str(), fv.c_str(), (int)di.deviceVolume, 
        (int)di.spareVolume, booklist);
    m_mutexForDeviceData.unlock();
}

// run in connect thread
void ConnectThread::checkBooklistChanged() 
{
    bool bookListChanged = false;

    m_mutexForBookListChanged.lock();
    bookListChanged = m_bookListChanged;
    m_mutexForBookListChanged.unlock();

    if (bookListChanged)
    {
        DeviceInfo di;
        list<ZDeviceBookInfo> booklist;

        m_mutexForBBHandle.lock();
        bool got = getDeviceInfo(di, booklist);
        m_mutexForBBHandle.unlock();

        if (got)
        {
            fillDeviceData(di, booklist);
            emit onBookListChanged();
        }
        m_mutexForBookListChanged.lock();
        m_bookListChanged = false;
        m_mutexForBookListChanged.unlock();
    }
}

// run in connect thread
void ConnectThread::checkConnected()
{
    bool gotDeviceInfo = false;
    DeviceInfo di;
    list<ZDeviceBookInfo> booklist;

    m_mutexForBBHandle.lock();
    BB_RESULT br = BambookConnect(DEFAULT_BAMBOOK_IP, 5000, &g_bbHandle);
    if (BR_SUCC == br)
        gotDeviceInfo = getDeviceInfo(di, booklist);
    m_mutexForBBHandle.unlock();

    if (gotDeviceInfo)
    {
        fillDeviceData(di, booklist);
        emit onConnected();
    }
}

// run in connect thread
bool ConnectThread::checkDisConnected()
{
    uint32_t status;
    bool isConnected = false;

    m_mutexForBBHandle.lock();
    if (BR_SUCC == BambookGetConnectStatus(g_bbHandle, &status))
    {
        if (status == CONN_CONNECTED)
            isConnected = true;
    }
    if (!isConnected)
    {
        BambookDisconnect(g_bbHandle);
        g_bbHandle = NULL;
    }
    m_mutexForBBHandle.unlock();

    if (g_bbHandle == NULL)
        return true;

    return false;
}

// run in connect thread
void ConnectThread::run()
{
    while(true)
    {
        if (m_abort)
            break;
        if (g_bbHandle == NULL)
            checkConnected();
        else
        {
            if (checkDisConnected())
                emit onDisConnected();
            else
                checkBooklistChanged();
        }
        msleep(g_bbHandle ? 500 : 50);
    }
    if (g_bbHandle != NULL)
    {
        BambookDisconnect(g_bbHandle);
        g_bbHandle = NULL;
    }
}

// in main thread
void ConnectThread::transferFromDevice(const wchar_t* bookId, const wchar_t* snbPath, void* cbParam)
{
    if (g_bbHandle == NULL)
        return;
    m_mutexForBBHandle.lock();
    CallbackUserData* data = new CallbackUserData();
    data->fromDevice = true;
    data->book = bookId;
    data->notifyBooklistChanged = false;
    data->oldData = cbParam;
    QByteArray buf, buf2;
    BB_RESULT result = BambookFetchPrivBook(g_bbHandle, wstr2str(buf, bookId), wstr2str(buf2, snbPath), 
        gTransCallback, (intptr_t2) data);
    Q_ASSERT(result == BR_SUCC);
    m_mutexForBBHandle.unlock();
}

// in main thread
void ConnectThread::transferToDevice(const wchar_t* snbFile, bool replace, 
    const wchar_t* replacedBookId, void* cbParam, bool notifyBooklistChanged)
{
    if (g_bbHandle == NULL)
        return;
    m_mutexForBBHandle.lock();
    CallbackUserData* data = new CallbackUserData();
    data->fromDevice = false;
    data->book = snbFile;
    data->notifyBooklistChanged = notifyBooklistChanged;
    data->oldData = cbParam;
    QByteArray buf,buf2;
    BB_RESULT result;
    if (replace)
    {
        result = BambookReplacePrivBook(g_bbHandle, wstr2str(buf, snbFile), 
            wstr2str(buf2, replacedBookId), gTransCallback, (intptr_t2)data);
        Q_ASSERT(result == BR_SUCC);
    }
    else
    {
        result = BambookAddPrivBook(g_bbHandle, wstr2str(buf, snbFile), gTransCallback, 
            (intptr_t2)data);
        Q_ASSERT(result == BR_SUCC);
    }
    m_mutexForBBHandle.unlock();
}

// in main thread
void ConnectThread::deleteDeviceBook(const wchar_t* bookId, bool notifyBooklistChanged)
{
    if (g_bbHandle == NULL)
        return;
    m_mutexForBBHandle.lock();
    QByteArray buf;
    BB_RESULT result = BambookDeletePrivBook(g_bbHandle, wstr2str(buf, bookId));
    Q_ASSERT(result == BR_SUCC);
    m_mutexForBBHandle.unlock();

    if(result == BR_SUCC && notifyBooklistChanged)
    {
        m_mutexForBookListChanged.lock();
        m_bookListChanged = true;
        m_mutexForBookListChanged.unlock();
    }
}

void ConnectThread::setBookListChanged()
{
    m_mutexForBookListChanged.lock();
    m_bookListChanged = true;
    m_mutexForBookListChanged.unlock();
}

bool packSnb(const wchar_t * snbName, const wchar_t * rootDir)
{
    QByteArray buf, buf2;
    return (BR_SUCC == BambookPackSnbFromDir(wstr2str(buf, snbName), wstr2str(buf2, rootDir)));
}

bool unpackSnb(const wchar_t * snbName, const wchar_t * relativePath, const wchar_t * outputName)
{
    QByteArray buf, buf2, buf3;
    BB_RESULT r = BambookUnpackFileFromSnb(wstr2str(buf, snbName), wstr2str(buf2, relativePath), 
        wstr2str(buf3, outputName));
    return (r == BR_SUCC);
}

bool verifySnb(const wchar_t * snbName)
{
    QByteArray buf;
    return (BR_SUCC == BambookVerifySnbFile(wstr2str(buf, snbName)));
}
