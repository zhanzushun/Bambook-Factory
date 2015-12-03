#ifndef PDF_H
#define PDF_H

#ifdef PDFEXPORTER_EXPORTS
#define PDF_API __declspec(dllexport)
#else
#define PDF_API __declspec(dllimport)
#endif


#define PHASE_ERROR -1
#define PHASE_AllDone 0
#define PHASE_1_CreateFolders 100
#define PHASE_2_CreateConfig 101
#define PHASE_3_ExtractText 102
#define PHASE_4_ReadHeadInfo 103
#define PHASE_5_ExtractImages 104
#define PHASE_6_CreatePage 105
#define PHASE_7_ChooseChapters 106

// require QString, QVector, QMutex

struct ChapterInfo
{
    bool used;
    bool byPage;
    int pageIndex;
    int rowIndex; // if byPage == false, then rowIndex means the index in whole text file
    QString title;
    QString context;
    ChapterInfo():used(true), byPage(false), pageIndex(-1), rowIndex(-1), title(""), context("") {}
};

typedef void(* ZOnConvertingCallack)(int phase, int progress, QString msg);

extern "C" PDF_API
void ZGetChapters(QVector<ChapterInfo>*& chapters1, QVector<ChapterInfo>*& chapters2, 
    QMutex*& mutex, bool*& useLeft, bool*& pRetry);


extern "C" PDF_API
bool ZConvertPdf(wchar_t* snbRootDir, int bufLen, const wchar_t* xpdfPath0, 
    const wchar_t* pdfFile0, const wchar_t* userDir0, bool isColumnMode, bool extractImages,
    int chapterType, bool* abort, ZOnConvertingCallack cb);

extern "C" PDF_API
    bool ZConvertTxt(wchar_t* snbRootDir, int bufLen, 
    const wchar_t* txtFile0, const wchar_t* userDir0, bool* abort, ZOnConvertingCallack cb);

#endif