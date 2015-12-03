#ifndef DISPLAY_H
#define DISPLAY_H

#include <string>
#include <list>
using namespace std;

struct ZBookInfo
{
    ZBookInfo();
    wstring id;
    wstring name;
    wstring author;
    wstring language;
    wstring rights;
    wstring publisher;
    wstring generator;
    wstring created;
    wstring abstract;
    wstring cover;
    list<pair<wstring, wstring> > chapters;
};

const wchar_t * ZGetSnbDir();
const wchar_t * ZGetSnbTempDir();
const wchar_t * ZGetHomeDir();

bool ZGetBookInfo(const wchar_t* snbFile, ZBookInfo& bookInfo);
bool ZReadBook(const wchar_t* snbFile, bool forceReflash = false);

#endif