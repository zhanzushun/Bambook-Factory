#ifndef CONNECT_H
#define CONNECT_H

#include <list>
#include <string>
using namespace std;

struct ZDeviceBookInfo
{
    wstring id;
    wstring name;
    wstring author;
    wstring abstract;
};

typedef void(* ZOnConnectedCB)(const wchar_t* sn, const wchar_t* fv, int totalSpaceInKb, 
    int freeSpaceInKb, const list<ZDeviceBookInfo>& booklist);

typedef void(* ZOnBooklistChangedCB)(int totalSpaceInKb, int freeSpaceInKb, 
    const list<ZDeviceBookInfo>& booklist);

typedef void(* ZOnDisConnectedCB)();

typedef void(* ZTranFromDeviceCB)(int status, int progress, const wchar_t* bookId, void* cbParam);

typedef void(* ZTransToDeviceCB)(int status, int progress, const wchar_t* snbFile, void* cbParam);

bool ZStartConnectThread(ZOnConnectedCB cb1, ZOnBooklistChangedCB cb2, ZOnDisConnectedCB cb3);

bool ZTransFromDevice(const wchar_t* id, const wchar_t* path, ZTranFromDeviceCB cb, void* cbParam);

bool ZTransToDevice(const wchar_t* snbFile, bool replace, const wchar_t* replacedBookId,
    ZTransToDeviceCB callback, void* cbParam, bool notifyBooklistChanged);

bool ZDeleteDeviceBook(const wchar_t* bookId, bool notifyBooklistChanged);

bool ZStopConnectThread(bool bForce = false);

#endif
