#ifndef CONNECTTHREAD_H
#define CONNECTTHREAD_H

#include "zconnect.h"
#include <QtCore/QThread>
#include <QtCore/QMutex>

struct DeviceInfo; // from BambookCore.h

class DeviceData
{
public:
    DeviceData();
    DeviceData(const DeviceData &other);
    ~DeviceData();
    void init(const wchar_t* sn, const wchar_t* fv, int ts, int fs, 
        const list<ZDeviceBookInfo> & bookList);

    const wchar_t* sn() const {return m_sn.c_str();}
    const wchar_t* fv() const {return m_fv.c_str();}
    int totalSpaceInKb() const {return m_totalSpaceInKb;}
    int freeSpaceInKb() const {return m_freeSpaceInKb;}
    const list<ZDeviceBookInfo> & bookList() const {return m_booklist;}
private:
    wstring m_sn;
    wstring m_fv;
    int m_totalSpaceInKb;
    int m_freeSpaceInKb;
    list<ZDeviceBookInfo> m_booklist;
};

class ConnectThread : public QThread
{
    Q_OBJECT

public:
    ConnectThread(QObject *parent = 0);
    ~ConnectThread();
    void stopProcess();
    QMutex & deviceMutex() {return m_mutexForDeviceData;}
    const DeviceData & deviceData() {return m_deviceData;}

    void transferFromDevice(const wchar_t* bookId, const wchar_t* snbPath, void* cbParam);
    void transferToDevice(const wchar_t* snbFile, bool replace, const wchar_t* replacedBookId,
        void* cbParam, bool notifyBooklistChanged);
    void deleteDeviceBook(const wchar_t* bookId, bool notifyBooklistChanged);
    void setBookListChanged();

Q_SIGNALS:
    void onConnected();
    void onDisConnected();
    void onBookListChanged();

protected:
    void run();

private:
    void checkBooklistChanged();
    void checkConnected();
    bool checkDisConnected();

    void fillDeviceData(const DeviceInfo& di, const list<ZDeviceBookInfo>& booklist);
    bool getDeviceInfo(DeviceInfo& di, list<ZDeviceBookInfo>& booklist);

    bool m_abort;
    DeviceData m_deviceData;
    bool m_bookListChanged;

    QMutex m_mutexForAbort;
    QMutex m_mutexForDeviceData;
    QMutex m_mutexForBBHandle;
    QMutex m_mutexForBookListChanged;
};

class ConnectThreadHost : public QObject
{
    Q_OBJECT
public:
    ConnectThreadHost(QObject *parent = 0);
    void init();

public Q_SLOTS:
    void onConnected();
    void onBookListChanged();
    void onDisConnected();
    void onTransfering(int status, int progress, void* userData);
};

class BambookTransferCallbackSender : public QObject
{
    Q_OBJECT
public:
    BambookTransferCallbackSender(QObject *parent = 0) {}
    void emitTransfering(int status, int progress, void* userData);
Q_SIGNALS:
    void onTransfering(int status, int progress, void* userData);
};

#endif
