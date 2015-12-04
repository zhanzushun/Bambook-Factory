// PdfExporter.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "zpdf.h"

QVector<ChapterInfo> g_chapters1, g_chapters2;
QMutex g_mutex;
bool g_useLeft;
bool g_retry;

extern "C" PDF_API
    void ZGetChapters(QVector<ChapterInfo>*& chapters1, QVector<ChapterInfo>*& chapters2, 
    QMutex*& mutex, bool*& pUseLeft, bool*& pRetry)
{
    chapters1 = &g_chapters1;
    chapters2 = &g_chapters2;
    mutex = &g_mutex;
    pUseLeft = &g_useLeft;
    pRetry = &g_retry;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

#define BUFSIZE 4096

bool* g_pToBeAbort = NULL;

inline bool toBeAbort()
{
    if (g_pToBeAbort)
        return *g_pToBeAbort;
    return false;
}

#define CHECK_ABORT if (toBeAbort()) return;
#define CHECK_ABORT_RTBOOL if (toBeAbort()) return false;

static ZOnConvertingCallack g_onConvertingCB = NULL;

inline void OnConverting(int phase, int progress = 0, QString msg = "")
{
    if (g_onConvertingCB != NULL)
        g_onConvertingCB(phase, progress, msg);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

namespace T
{
    void NormalizePath(wstring& path, LPCTSTR str)
    {
        TCHAR buf[BUFSIZE];
        PathCanonicalize(buf, str);
        path = buf;
        if (path.empty())
            return;
        if (path[path.length() - 1] != _T('\\'))
            path += _T("\\");
    }

    inline void ReplaceStringOneTime(wstring &target, const wstring &that, const wstring &with) 
    {
        wstring::size_type where = target.find(that);
        if(where != wstring::npos) 
        {
            target.replace(target.begin() + where,
                target.begin() + where + that.size(),
                with.begin(),
                with.end());
        }
    }

    inline void SplitString(const wstring& str0, const wstring& delim, vector<wstring>& results)
    {
        wstring str = str0;
        int cutAt;
        while( (cutAt = str.find_first_of(delim)) != str.npos )
        {
            if(cutAt > 0)
            {
                results.push_back(str.substr(0,cutAt));
            }
            str = str.substr(cutAt+1);
        }
        if(str.length() > 0)
        {
            results.push_back(str);
        }
    }

    inline QString wstr2qstr(const wchar_t* wstr)
    {
        if (sizeof(wchar_t) == sizeof(unsigned short))
            return QString::fromUtf16((const ushort *)wstr);
        else
        {
            Q_ASSERT(false);
            return QString::fromUcs4((const uint *)wstr);
        }
    }

    inline wstring& qstr2wstr(wstring& wstr, const QString &string) 
    {
        if (sizeof(wchar_t) == sizeof(unsigned short))
            wstr = (const wchar_t*) string.unicode();
        else
            Q_ASSERT(false);
        return wstr;
    }

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

    inline void str2wstr(wstring & wstr, const char* str)
    {
        QByteArray encodedString = str;
        QString string = CODEC()->toUnicode(encodedString);
        wstr = qstr2wstr(wstr, string);
    }

    inline const char* wstr2str(QByteArray & buf, const wchar_t* wstr)
    {
        buf = CODEC()->fromUnicode(wstr2qstr(wstr));
        return buf.constData();
    }

    // trim from both ends
    inline wstring &Trim(wstring &str) 
    {
        size_t endpos = str.find_last_not_of(_T(" \t\n\r"));
        if( wstring::npos != endpos )
        {
            str = str.substr( 0, endpos+1 );
        }
        size_t startpos = str.find_first_not_of(_T(" \t\n\r"));
        if( wstring::npos != startpos )
        {
            str = str.substr( startpos );
        }
        return str;
    }

    inline QString GetFileNameWithoutExt(const QString& fileName)
    {
        QString ext = ".pdf";
        return fileName.mid(0, fileName.length() - ext.length());
    }

    inline void GetFileInfo(const QString& fullPathFile, QString& dir, QString& fileName, QString& fileNameWithoutExt)
    {
        QFileInfo fi(fullPathFile);
        fileName = fi.fileName();
        dir = fi.absolutePath();
        dir.replace('/', '\\');
        fileNameWithoutExt = GetFileNameWithoutExt(fileName);
    }

    inline void GetFileInfo(LPCTSTR fullPathFile, QString& dir, QString& fileName, QString& fileNameWithoutExt)
    {
        GetFileInfo(wstr2qstr(fullPathFile), dir, fileName, fileNameWithoutExt);
    }

    inline QString GetFileDir(LPCTSTR fullPathFile)
    {
        QString dir, filename, fnw;
        GetFileInfo(fullPathFile, dir, filename, fnw);
        return dir;
    }

    inline QString GetFileNameWithoutExt(LPCTSTR fullPathFile)
    {
        QString dir, filename, fnw;
        GetFileInfo(fullPathFile, dir, filename, fnw);
        return fnw;
    }

    inline QString GetFileName(LPCTSTR fullPathFile)
    {
        QString dir, filename, fnw;
        GetFileInfo(fullPathFile, dir, filename, fnw);
        return filename;
    }

    inline QString GetFileDir(const QString& fullPathFile)
    {
        QString dir, filename, fnw;
        GetFileInfo(fullPathFile, dir, filename, fnw);
        return dir;
    }

    inline QString GetFileName(const QString& fullPathFile)
    {
        QString dir, filename, fnw;
        GetFileInfo(fullPathFile, dir, filename, fnw);
        return filename;
    }

    bool CleanDirectory(QDir &aDir, bool delMe = false)
    {
        if (aDir.exists())//QDir::NoDotAndDotDot
        {
            QFileInfoList entries = aDir.entryInfoList(QDir::NoDotAndDotDot | 
                QDir::Dirs | QDir::Files);
            int count = entries.size();
            for (int idx = 0; ((idx < count)); idx++)
            {
                QFileInfo entryInfo = entries[idx];
                QString path = entryInfo.absoluteFilePath();
                if (entryInfo.isDir())
                {
                    CleanDirectory(QDir(path), true);
                }
                else
                {
                    QFile file(path);
                    file.remove();
                }
            }
            if (delMe)
                return (aDir.rmdir(aDir.absolutePath()));
            else
            {
                entries = aDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);
                return (entries.size() == 0);
            }
        }
        return true;
    }

    inline void EnsureExistDir(const QString& dir, const QString& subdir)
    {
        QDir d(dir);
        if (!d.exists(subdir))
            d.mkdir(subdir);
    }

    int GetDirImgCount(const QString& dir, bool& allImage)
    {
        QDir dirImgOut(dir);
        QFileInfoList entries = dirImgOut.entryInfoList(QDir::NoDotAndDotDot | QDir::Files);
        int count = entries.size();
        int jpgCount = 0;
        allImage = true;
        for (int idx = 0; ((idx < count)); idx++)
        {
            QFileInfo entryInfo = entries[idx];
            QString fileName = entryInfo.fileName();
            if (entryInfo.isDir())
            {
                allImage = false;
                continue;
            }
            if ((fileName.right(4) == ".jpg" || fileName.right(4) == ".ppm") && fileName.left(1) == "-")
                jpgCount++;
        }
        if (jpgCount != count)
            allImage = false;
        return jpgCount;
    }
}

using namespace T;

namespace G
{
    HWND WaitWindow(DWORD processId, HWND parent, LPCTSTR clsName, LPCTSTR wndName, 
        LPCTSTR wndName2 = NULL, int timeout = 3000)
    {
        ULONGLONG curTime = GetTickCount64();
        HWND hwnd = NULL;
        while(true)
        {
            if ((GetTickCount64() - curTime) > timeout) // if > 3 seconds, then failed
                return NULL;

            Sleep(10);
            if (parent != NULL)
            {
                hwnd = FindWindowEx(parent, 0, clsName, wndName);
                if (hwnd == NULL && wndName2 != NULL)
                    hwnd = FindWindowEx(parent, 0, clsName, wndName2);
            }
            else
            {
                hwnd = FindWindow(clsName, wndName);
                if (hwnd == NULL && wndName2 != NULL)
                    hwnd = FindWindow(clsName, wndName2);
            }
            if (hwnd != NULL)
            {
                if (parent != NULL)
                    return hwnd;
                if (processId == NULL)
                    return hwnd;
                DWORD pid = 0;
                GetWindowThreadProcessId(hwnd, &pid);
                if (pid == processId)
                    return hwnd;
                hwnd = NULL;
            }
        }
        return NULL;
    }

    bool WaitNoWindow(DWORD processId, LPCTSTR clsName, LPCTSTR wndName, int timeout = 3000)
    {
        ULONGLONG curTime = GetTickCount64();    
        HWND hwnd = NULL;
        while(true)
        {
            if ((GetTickCount64() - curTime) > timeout) // if > timeout, then failed
                return false;

            Sleep(10);
            hwnd = FindWindowEx(NULL, NULL, clsName, wndName);
            if (hwnd == NULL)
                return true;
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            while (hwnd != NULL && pid != processId)
            {
                hwnd = FindWindowEx(NULL, hwnd, clsName, wndName);
                if (hwnd != NULL)
                    GetWindowThreadProcessId(hwnd, &pid);
            }
            if (hwnd == NULL)
                return true;
            if (pid == processId)
                hwnd = NULL;
        }
        return false;
    }

    bool StartProcess(LPCTSTR cmdLine, PROCESS_INFORMATION& processInfo)
    {
        STARTUPINFO startInfo;
        memset(&startInfo, 0, sizeof(startInfo));
        startInfo.cb = sizeof(STARTUPINFO);
        startInfo.dwFlags = STARTF_USESHOWWINDOW;
        startInfo.wShowWindow = SW_HIDE;

        TCHAR buf[BUFSIZE];
        _tcscpy(buf, cmdLine);

        if (!CreateProcess( NULL, buf, NULL, NULL, FALSE,
            CREATE_NEW_CONSOLE, 
            NULL, 
            NULL,
            &startInfo,
            &processInfo))
        {
            return false;
        }
        return true;
    }

    void EndProcess(bool bWait, const PROCESS_INFORMATION& processInfo)
    {
        try
        {
            if (bWait)
                WaitForSingleObject(processInfo.hProcess, INFINITE);
            TerminateProcess(processInfo.hProcess, 0);
            CloseHandle(processInfo.hThread);
            CloseHandle(processInfo.hProcess);
        }
        catch (...)
        {
        }
    }

    class CAutoStartProcess
    {
        PROCESS_INFORMATION m_pi;
        bool m_valid;
        bool m_wait;
    public:
        CAutoStartProcess(LPCTSTR cmdLine, bool wait) { m_valid = StartProcess(cmdLine, m_pi); m_wait = wait;}
        ~CAutoStartProcess() {End();}
        void End() {if (m_valid) EndProcess(m_wait, m_pi); m_valid = false;}
        bool IsValid() {return m_valid;}
        const PROCESS_INFORMATION& Info() const {return m_pi;}
    };

    bool RunCmdStrOut(TCHAR* psCmdLine, DWORD* out_exitcode, std::wstring& strOut)
    {
        static const int MAX_CMD = 1024;

        STARTUPINFO si = { sizeof(STARTUPINFO) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        BOOL bUsePipes = FALSE;
        HANDLE FWritePipe = NULL;
        HANDLE FReadPipe = NULL;
        SECURITY_ATTRIBUTES pa = { sizeof(pa), NULL, TRUE };
        bUsePipes = ::CreatePipe( &FReadPipe, &FWritePipe, &pa, 0 );
        if ( bUsePipes != FALSE )
        {
            si.hStdOutput = FWritePipe;
            si.hStdInput = FReadPipe;
            si.hStdError = FWritePipe;
            si.dwFlags = STARTF_USESTDHANDLES | si.dwFlags;
        }

        PROCESS_INFORMATION pi = { 0 };
        BOOL RetCode = ::CreateProcess( NULL,
            psCmdLine,
            NULL, // Process handle not inheritable
            NULL, // Thread handle not inheritable
            TRUE, // Set handle inheritance to FALSE
            0, // No creation flags
            NULL, // Use parent's environment block
            NULL, //path.GetDirectory(),
            &si, // STARTUPINFO
            &pi ); // PROCESS_INFORMATION

        if ( RetCode == FALSE )
        {
            ::CloseHandle( FReadPipe );
            ::CloseHandle( FWritePipe );
            return false;
        }

        ::CloseHandle( pi.hThread );

        try
        {
            DWORD BytesToRead = 0;
            DWORD BytesRead = 0;
            DWORD TotalBytesAvail = 0;
            DWORD PipeReaded = 0;
            DWORD exit_code = 0;
            while ( ::PeekNamedPipe( FReadPipe, NULL, 0, &BytesRead, &TotalBytesAvail, NULL ) )
            {
                if ( TotalBytesAvail == 0 )
                {
                    if ( ::GetExitCodeProcess( pi.hProcess, &exit_code ) == FALSE ||
                        exit_code != STILL_ACTIVE )
                    {
                        break;
                    }
                    else
                    {
                        continue;
                    }
                }
                else
                {
                    while ( TotalBytesAvail > BytesRead )
                    {
                        if ( TotalBytesAvail - BytesRead > MAX_CMD - 1 )
                        {
                            BytesToRead = MAX_CMD - 1;
                        }
                        else
                        {
                            BytesToRead = TotalBytesAvail - BytesRead;
                        }
                        char buf[1024];
                        if ( ::ReadFile( FReadPipe,
                            buf,
                            BytesToRead,
                            &PipeReaded,
                            NULL ) == FALSE )
                        {
                            break;
                        }
                        if ( PipeReaded <= 0 ) continue;
                        BytesRead += PipeReaded;
                        buf[ PipeReaded ] = '\0';
                        wstring tempstr;
                        str2wstr(tempstr, buf);
                        strOut += tempstr;
                    }
                }
            }
        }
        catch (...)
        {
        }

        ::GetExitCodeProcess( pi.hProcess, out_exitcode );
        ::CloseHandle( pi.hProcess );
        ::CloseHandle( FReadPipe );
        ::CloseHandle( FWritePipe );
        return true;
    }
}

using namespace G;

bool pExtractPdfImages(LPCTSTR exeFile, LPCTSTR pdfFile)
{
    CAutoStartProcess process(exeFile, false);
    if (!process.IsValid())
        return false;

    HWND hwnd = WaitWindow(process.Info().dwProcessId, 0, NULL, _T("Some PDF Image Extract 1.5"));
    assert(hwnd != NULL);
    if (hwnd == NULL)
        return false;

    HWND hwndMain = FindWindowEx(FindWindowEx(FindWindowEx(hwnd, 0, 0, 0), 0, 0, 0), 0, _T("#32770"), 0);
    assert(hwndMain != NULL);
    if (hwndMain == NULL)
        return false;
    /*
    //open the current folder dialog, and get current folder
    PostMessage(hwndMain, WM_COMMAND, 0x8003, (LPARAM)GetDlgItem(hwndMain, 0x8003));
    HWND hwndFolderDlg = WaitWindow(process.Info().dwProcessId, 0, _T("#32770"), _T("Please Select a Folder"));
    assert(hwndFolderDlg != NULL);
    if (hwndFolderDlg == NULL)
        return false;
    char buf[BUFSIZE];
    
    HWND hwndCurFolderEdit = GetDlgItem(hwndFolderDlg, 0x66);
    int len = 0;
    while (len == 0)
    {
        Sleep(10);
        len = SendMessageA(hwndCurFolderEdit, WM_GETTEXT, (WPARAM)BUFSIZE, (LPARAM)buf);
    }
    QString curFolder = buf;
    wstring wsCurFolder;
    qstr2wstr(wsCurFolder, curFolder);
    
    // copy the pdf to current folder
    QFileInfo pdfFileInfo(wstr2qstr(pdfFile));
    QString pdfFileName = pdfFileInfo.fileName();
    wstring wsPdfFileName;
    qstr2wstr(wsPdfFileName, pdfFileName);

    wstring cmdCopyPdf = _T("cmd.exe /c copy \"") + wstring(pdfFile) + _T("\" \"") + wsCurFolder + _T("\"");
    CAutoStartProcess processCopyPdf(cmdCopyPdf.c_str(), true);
    if (!processCopyPdf.IsValid())
        return false;

    //close the current folder dialog
    SendMessage(hwndFolderDlg, WM_COMMAND, 0x02, (LPARAM)GetDlgItem(hwndMain, 0x02));
    bool result = WaitNoWindow(process.Info().dwProcessId, _T("#32770"), _T("Please Select a Folder"), 3000);
    if (!result)
        return false;
    */

    //open the file dialog
    PostMessage(hwnd, WM_COMMAND, 0x8002, 0);
    HWND hwndOpenDlg = WaitWindow(process.Info().dwProcessId, 0, _T("#32770"), _T("Open"));
    assert(hwndOpenDlg != NULL);
    if (hwndOpenDlg == NULL)
        return false;
    HWND hwndEdit = WaitWindow(0, hwndOpenDlg, _T("Edit"), NULL);
    assert(hwndEdit != NULL);
    if (hwndEdit == NULL)
        return false;

    //input the file name to file dialog
    Sleep(500);
    QByteArray byteArrayBuf;
    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)wstr2str(byteArrayBuf, pdfFile));
    SendMessage(hwndOpenDlg, WM_COMMAND, 1, (LPARAM)GetDlgItem(hwndOpenDlg, 1));

    //execute the extract
    bool result = WaitNoWindow(process.Info().dwProcessId, _T("#32770"), _T("Open"), 3000);
    if (!result)
        return false;
    SendMessage(hwndMain, WM_COMMAND, 0x800D, (LPARAM)GetDlgItem(hwndMain, 0x800D));

    return true;
}

bool ExtractPdfImages(int& jpgCount, LPCTSTR exeFile, LPCTSTR pdfFile, LPCTSTR outputDir)
{
    QString pdfDir, pdfFileName, pdfFileNameWithoutExt;
    GetFileInfo(pdfFile, pdfDir, pdfFileName, pdfFileNameWithoutExt);

    //get the output dir
    wstring wsPdfDir;
    qstr2wstr(wsPdfDir, pdfDir);
    wstring wsPdfDirWithSlash;
    if (!wsPdfDir.empty())
    {
        if (wsPdfDir[wsPdfDir.length()-1] == _T('\\'))
        {
            wsPdfDirWithSlash = wsPdfDir;
            wsPdfDir = wsPdfDir.substr(0, wsPdfDir.length()-1);
        }
        else
            wsPdfDirWithSlash = wsPdfDir + _T("\\");
    }

    /*
    //close the explorer of output.
    while(true)
    {
        HWND hwndExplorer = FindWindow(_T("CabinetWClass"), wsPdfDir.c_str());
        if (hwndExplorer == NULL)
            hwndExplorer = FindWindow(_T("CabinetWClass"), wsPdfDirWithSlash.c_str());
        if (hwndExplorer == NULL)
            break;
        SendMessage(hwndExplorer, WM_CLOSE, 0, 0);
    }
    */

    // call the main process
    if (!pExtractPdfImages(exeFile, pdfFile))
        return false;

    // remove the explorer
    HWND hwndExplorer = WaitWindow(0, 0, _T("CabinetWClass"), 
        //wsPdfDir.c_str(), wsPdfDirWithSlash.c_str(), 1000);
        NULL, NULL, 1000);
    if (hwndExplorer != NULL)
        SendMessage(hwndExplorer, WM_CLOSE, 0, 0);

    //move files to outDir
    wstring wsPdfFileNameWithoutExt;
    wstring wsImgOutDir = wsPdfDirWithSlash + qstr2wstr(wsPdfFileNameWithoutExt, pdfFileNameWithoutExt);

    QString imgOutDir = pdfDir + "\\" + pdfFileNameWithoutExt;
    bool allImage = true;
    jpgCount = GetDirImgCount(imgOutDir, allImage);

    wstring cmdCopy = _T("cmd.exe /c copy \"") + wsImgOutDir + _T("\\*.*\" \"") + outputDir + _T("\"");
    if (allImage)
        cmdCopy += _T(" & del /Q \"") + wsImgOutDir + _T("\\*.*\" & rmdir /Q \"") + wsImgOutDir + _T("\"");
        
    CAutoStartProcess processCopy(cmdCopy.c_str(), true);
    if (!processCopy.IsValid())
        return false;
    return true;
}

void CreateXpdfRcFile(const wstring& wstrXpdfPath, const wstring& rcPath)
{
    wstring wstrXpdfRc = wstrXpdfPath + _T("xpdfrc.std");
    wstring wstrXpdfRcOut = rcPath + _T("xpdfrc");
    vector<wstring> list;

    TCHAR buf[BUFSIZE];
    GetWindowsDirectory(buf, BUFSIZE);
    wstring wndsDir;
    NormalizePath(wndsDir, buf);

    TCHAR bufXpdfPath[BUFSIZE];
    GetShortPathName(wstrXpdfPath.c_str(), bufXpdfPath, BUFSIZE);

    //read string to list
    {
        wifstream file(wstrXpdfRc.c_str());
        if (file.is_open())
        {
            wstring line;
            while (file.good())
            {
                getline (file, line);
                ReplaceStringOneTime(line, _T(".\\zh"), wstring(bufXpdfPath) + _T("zh"));
                ReplaceStringOneTime(line, _T("C:\\WINDOWS\\"), wndsDir);
                list.push_back(line);
            }
            file.close();
        }
    }

    //write string to list
    {
        wofstream fileOut(wstrXpdfRcOut.c_str());
        if (fileOut.is_open())
        {
            vector<wstring>::const_iterator it = list.begin();
            for (; it!=list.end(); it++)
            {
                fileOut << (*it).c_str() << endl;
            }
            fileOut.close();
        }
    }
}

bool ExtractTxtFromPdf(const wstring& xpdfPath, LPCTSTR pdfFile, LPCTSTR pdfDir, LPCTSTR thisPdfDir
    , bool isColumnMode)
{
    wstring param = _T(" -cfg \"") + wstring(pdfDir) + _T("xpdfrc\" "); // config file
    param += _T("-layout -enc GBK "); // layout and encoding
    if (isColumnMode) param += _T("-raw "); // row mode
    param += _T("\"") + wstring(pdfFile) + _T("\" "); // input pdf file
    param += _T("\"") + wstring(thisPdfDir) + _T("pdfout.txt\" "); // output txt file

    wstring xpdf2textCmd = _T("\"") + xpdfPath + _T("pdftotext.exe\" ") + param;
    CAutoStartProcess process(xpdf2textCmd.c_str(), true);
    if (!process.IsValid())
        return false;
    return true;
}

struct PdfInfo
{
    wstring title; //Title
    wstring author; //Author
    wstring creator; //Creator
    wstring producer; //Producer
    wstring creationDate; //CreationDate
    wstring modDate; //ModDate
    int pages; //Pages
    wstring pageSize; //Page size
    wstring fileSize; // File size
    bool encrypted; //Encrypted, yes/no
    bool optimized; //Optimized, yes/no
    wstring pdfVersion; //PDF version
};

void AddToPdfInfo(const wstring& key, const wstring& value, PdfInfo& pdfInfo)
{
    if (key == _T("Title"))
        pdfInfo.title = value;
    else if (key == _T("Author"))
        pdfInfo.author = value;
    else if (key == _T("Creator"))
        pdfInfo.creator = value;
    else if (key == _T("Producer"))
        pdfInfo.producer = value;
    else if (key == _T("CreationDate"))
        pdfInfo.creationDate = value;
    else if (key == _T("ModDate"))
        pdfInfo.modDate = value;
    else if (key == _T("Pages"))
        pdfInfo.pages = _ttoi(value.c_str());
    else if (key == _T("Page size"))
        pdfInfo.pageSize = value;
    else if (key == _T("File size"))
        pdfInfo.fileSize = value;
    else if (key == _T("Encrypted"))
        pdfInfo.encrypted = (value == _T("yes")) ? true : false;
    else if (key == _T("Optimized"))
        pdfInfo.optimized = (value == _T("yes")) ? true : false;
    else if (key == _T("PDF version"))
        pdfInfo.pdfVersion = value;
}

bool GetPdfInfo(PdfInfo& pdfInfo, const wstring& xpdfPath, LPCTSTR pdfFile)
{
    wstring param = _T("\"") + wstring(pdfFile) + _T("\" "); // input pdf file
    wstring xpdfInfoCmd = _T("\"") + xpdfPath + _T("pdfinfo.exe\" ") + param;

    wstring strOut;
    DWORD exitCode;
    if (!RunCmdStrOut((LPTSTR)xpdfInfoCmd.c_str(), &exitCode, strOut))
        return false;
    if (strOut.empty())
        return false;

    vector<wstring> lines;
    SplitString(strOut, _T("\n"), lines);
    if (lines.size() <= 0)
        return false;
    for(size_t i=0; i<lines.size(); i++)
    {
        wstring line = lines[i];
        size_t pos = line.find_first_of(_T(':'));

        wstring key,value;
        if (pos != line.npos)
            key = line.substr(0, pos);
        if (pos != line.size()-1)
            value = line.substr(pos + 1);
        Trim(key);
        Trim(value);
        AddToPdfInfo(key, value, pdfInfo);
    }
    return true;
}

void pAddPageImages(QTextStream& writeStream, const QString& imagesDir, int page)
{
    QDir dir(QString("%1%2").arg(imagesDir).arg(page));
    //QDir dir(imagesDir);
    QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files);
    int count = entries.size();
    for (int idx = 0; ((idx < count)); idx++)
    {
        if (entries[idx].isDir())
            break;
        QString fileName = entries[idx].fileName();
        //int pos = fileName.indexOf("-");
        //if (pos != -1 && (fileName.right(4) == ".png") && fileName.left(pos).toInt() == page)
        if (fileName.right(4) == ".png")
        {
            writeStream 
                << "\n" 
                << "]]></text>"
                << QString("<img>/%1/%2</img>").arg(page).arg(fileName)
                << "<text><![CDATA["
                << "\n";
        }
    }
}

bool ppm2png(const QString& xpdfPath, const QString& dir0)
{
    QDir dir(dir0);
    QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files);
    int count = entries.size();
    for (int idx = 0; ((idx < count)); idx++)
    {
        if (entries[idx].isDir())
            break;
        QString fileName = entries[idx].fileName();
        if ((fileName.right(4) == ".ppm") && fileName.left(1) == "-")
        {
            QString input = dir0 + fileName;
            QString output = QString("%1%2%3").arg(dir0).
                arg(fileName.mid(0, fileName.length() - 4)).arg(".png");

            QString cmd = QString("\"%1convert\\convert.exe\" \"%2\" \"%3\"").
                arg(xpdfPath).arg(input).arg(output);
            {
                wstring wsCmd;
                CAutoStartProcess process(qstr2wstr(wsCmd, cmd).c_str(), true);
                if (!process.IsValid())
                    return false;
            }
            QFile file(entries[idx].absoluteFilePath());
            file.remove();
        }
    }
    return true;
}

class PageThread : public QThread
{
public:
    PageThread(QObject *parent = 0):QThread(parent){}
    ~PageThread(){}
    void Start(int index, int pageStart, int pageEnd, const QString& xpdfPath, const QString& pdfFile,
        const QString& imagesDir, const QString& imagesDirShort)
    {
        m_index = index;
        m_pageStart = pageStart;
        m_pageEnd = pageEnd;
        m_xpdfPath = xpdfPath;
        m_imagesDir = imagesDir;
        m_imagesDirShort = imagesDirShort;
        m_pdfFile = pdfFile;
        start();
    }
private:
    int m_index;
    int m_pageStart;
    int m_pageEnd;
    QString m_xpdfPath;
    QString m_imagesDir;
    QString m_imagesDirShort;
    QString m_pdfFile;
protected:
    void run()
    {
        for (int i=m_pageStart; i<=m_pageEnd; i++)
        {
            QString subDir = QString("%1").arg(i);
            EnsureExistDir(m_imagesDir, subDir);
            QString currentDir = m_imagesDir + subDir + "\\";
            QString currentDirShort = m_imagesDirShort + subDir + "\\";

            CHECK_ABORT;
            int progress = (i+1-m_pageStart)*100/(m_pageEnd-m_pageStart+1);

            OnConverting(PHASE_5_ExtractImages, 0, QString(QCoreApplication::translate("PDFExporter", "Thread %1, progress %2, extracting images from page %3..."))
                    .arg(m_index+1).arg(progress).arg(i));

            QString cmd = QString("\"%1pdfimages.exe\" -q -f %2 -l %3 \"%4\" %5").
                arg(m_xpdfPath).arg(i).arg(i).arg(m_pdfFile).
                arg(currentDirShort);
            {
                wstring wsCmd;
                CAutoStartProcess process(qstr2wstr(wsCmd, cmd).c_str(), true);
                if (!process.IsValid())
                    return;
            }
            ppm2png(m_xpdfPath, currentDir);
            CHECK_ABORT;
            if (m_index != 10)
                OnConverting(PHASE_5_ExtractImages, 0, QString(QCoreApplication::translate("PDFExporter", "Thread %1, progress %2, extracted images from page %3 successfully."))
                    .arg(m_index+1).arg(progress).arg(i));
        }
    }
};

bool ExtractPdfImages2(int pages, const wstring& xpdfPath, LPCTSTR pdfFile, const QString& imagesDir)
{
    wstring wsOut;
    TCHAR buf[BUFSIZE];
    GetShortPathName(qstr2wstr(wsOut, imagesDir).c_str(), buf, BUFSIZE);
    CleanDirectory(QDir(imagesDir));

    int threadCount = 10;
    int pagesPerThread = pages / threadCount;
    int pagesForLastThread = pages % threadCount;
    threadCount ++;

    QVector<PageThread*> threads;
    for (int i = 0; i< threadCount; i++)
    {
        threads.push_back(new PageThread());

        int pageStart = i * pagesPerThread;
        int pageEnd = i * pagesPerThread + pagesPerThread - 1;
        if (i == threadCount - 1)
            pageEnd = pages - 1;

        threads[i]->Start(i, pageStart+1, pageEnd+1, wstr2qstr(xpdfPath.c_str()), 
            wstr2qstr(pdfFile), imagesDir, wstr2qstr(buf));
    }

    while (true)
    {
        bool completed = true;
        for (int i = 0; i< threadCount; i++)
        {
            if (!threads[i]->wait(1))
                completed = false;
        }
        if (completed)
            break;
    }
    return true;
}

void pAddBlockToStream(QTextStream& writeStream, const QList<QPair<QString, int> >& block, bool insertBlankLine)
{
    if (block.count() == 0)
        return;

    if (block.count() == 1)
    {
        writeStream << block.at(0).first << "\n";
        return;
    }
    int minSpaceCount = block.at(0).second;
    //calculate space count
    {
        QPair<QString, int> line;
        foreach (line, block)
        {
            if (line.second < minSpaceCount)
                minSpaceCount = line.second;
        }
    }
    //print the line
    {
        QPair<QString, int> line;
        bool first = true;
        foreach (line, block)
        {
            if (first)
            {
                writeStream << line.first.mid(minSpaceCount);
                first = false;
            }
            else
                writeStream << line.first.mid(line.second);

            // add a space if not CKJ
            if (line.first.length() > 0 && line.first[line.first.length() -1] < 256)
                writeStream << " ";
        }
    }
    writeStream << "\n";
    if (insertBlankLine)
        writeStream << " \n";
}

inline void pCompleteBlock(QList<QPair<QString, int> >& block, QTextStream& writeStream,
    const QString& strLine, const QVector<int>& spaceCountList, int i, bool insertBlankLine)
{
    block << qMakePair(strLine, spaceCountList[i]);
    pAddBlockToStream(writeStream, block, insertBlankLine);
    block.clear();
}

inline void pAddToBlock(QList<QPair<QString, int> >& block, 
    const QString& strLine, const QVector<int>& spaceCountList, int i)
{
    block << qMakePair(strLine, spaceCountList[i]);
}

void pAddPage(QTextStream& writeStream, const QVector<QString>& page, bool insertBlankLine)
{
    QVector<int> spaceCountList;
    QVector<int> lineLenghList;
    foreach (const QString& strLine, page)
    {
        int lineLen = 0;
        bool startSpace = true;
        int startSpaceCount = 0;
        int avalibleLineLenCount = 0; // need to > 2

        for (int i=0; i<strLine.length(); i++)
        {
            lineLen += (strLine[i].unicode() < 256) ? 1 : 2; // for CKJ the length*2
            if (!strLine[i].isSpace())
                startSpace = false;
            if (startSpace)
                startSpaceCount ++;
        }
        spaceCountList.push_back(startSpaceCount);
        lineLenghList.push_back(lineLen);
    }

    int maxLineLen = 0; // need to remove noisy, max to 2 lines noisy.
    QVector<int> sortedLineLenghList(lineLenghList);
    qSort(sortedLineLenghList.begin(), sortedLineLenghList.end(), qGreater<int>());
    if (page.count() > 0)
        maxLineLen = sortedLineLenghList[0];
    if (page.count() > 4)
    {
        if (sortedLineLenghList[1] * 10 < maxLineLen * 9)
        {
            maxLineLen = sortedLineLenghList[1];
            if (sortedLineLenghList[2] * 10 < maxLineLen * 9)
                maxLineLen = sortedLineLenghList[2];
        }
    }

    QList<QPair<QString, int> > block;
    for (int i=0; i<page.count(); i++)
    {
        QString strLine = page[i];
        // if last line, then completes the block
        if (i == page.count() -1)
        {
            pCompleteBlock(block, writeStream, strLine, spaceCountList, i, insertBlankLine);
            continue;
        }
        // if current line is empty, then completes the block
        if (strLine.trimmed().isEmpty())
        {
            pCompleteBlock(block, writeStream, " ", spaceCountList, i, insertBlankLine);
            continue;
        }
        QRegExp rx(STR("\\s*(\\s?[\\.．。]){10,}\\s*"));
        // if current line is an index line, then completes the block
        if (rx.indexIn(strLine) != -1)
        {
            QString strCopy = strLine; 
            strCopy.replace(rx, "..........");
            strCopy = "  " + strCopy.trimmed();
            pCompleteBlock(block, writeStream, strCopy, spaceCountList, i, insertBlankLine);
            continue;
        }
        // get next line
        QString nextLine;
        if (i != page.count() -1)
            nextLine = page[i+1];
        // if next line is empty, then completes the block
        if (nextLine.trimmed().isEmpty())
        {
            pCompleteBlock(block, writeStream, strLine, spaceCountList, i, insertBlankLine);
            continue;
        }
        // if next line is an index line, then completes the block
        if (rx.indexIn(nextLine) != -1)
        {
            pCompleteBlock(block, writeStream, strLine, spaceCountList, i, insertBlankLine);
            continue;
        }
        // if current line ends with ":", then completes the block
        if (strLine.right(1) == ":")
        {
            pCompleteBlock(block, writeStream, strLine, spaceCountList, i, insertBlankLine);
            continue;
        }
        // if next line has an indent, then completes the block
        if (i < page.count() -1 && spaceCountList[i] < spaceCountList[i+1])
        {
            pCompleteBlock(block, writeStream, strLine, spaceCountList, i, insertBlankLine);
            continue;
        }
        // if the length of current line > 0.9 maxlen, then continue the block
        if (lineLenghList[i]*10 > maxLineLen * 8)
        {
            pAddToBlock(block, strLine, spaceCountList, i);
            continue;
        }
        // if the length of current line + next word of next line, then continue the block
        if (lineLenghList[i] *10 > maxLineLen * 7)
        {
            nextLine = nextLine.trimmed();
            if (nextLine[0].unicode() < 256)
            {
                QRegExp rxAword("\\b\\w+\\b");
                int pos = rxAword.indexIn(nextLine);
                if (pos != -1)
                {
                    if ((lineLenghList[i] + rxAword.matchedLength()) * 10 > maxLineLen * 9)
                    {
                        pAddToBlock(block, strLine, spaceCountList, i);
                        continue;
                    }
                }
            }
        }
        // not special case, completes the block
        pCompleteBlock(block, writeStream, strLine, spaceCountList, i, insertBlankLine);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

QTextStream* pSnbcStart(QFile*& pOutFile, const QString& fileName, const QString& snbcDir, const QString& snbcTitle)
{
    pOutFile = new QFile(snbcDir + fileName);
    pOutFile->open(QIODevice::WriteOnly);
    QTextStream* pWriteStream = new QTextStream(pOutFile);
    QTextStream& writeStream = *pWriteStream;
    writeStream.setCodec("UTF-8");
    writeStream << "<snbc><head><title><![CDATA[" << snbcTitle << "]]></title>";
    writeStream << "</head><body><text><![CDATA[ ";
    return pWriteStream;
}

void pSnbcEnd(QTextStream* pWriteStream, QFile* pOutFile)
{
    QTextStream& writeStream = *pWriteStream;
    writeStream << "]]></text></body></snbc>";
    delete pWriteStream;
    delete pOutFile;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool CheckNewChapter(const QString& pageStart)
{
    //QRegExp ex(STR("\\s*第\\s*(\\d|一|二|三|四|五|六|七|八|九|十|零|百|千){1,20}\\s*(章|节|回|集|卷)[\\.;:(\\r|(\\r?\\n)|\\n)\\t：；、\\s]"));
    QRegExp ex(STR("\\s*(卷|第){1}\\s*(\\d|一|二|三|四|五|六|七|八|九|十|零|百|千){1,20}\\s*(章|节|回|集|卷|部)"));
    if (ex.indexIn(pageStart) != -1)
    {
        if (pageStart.length() > 40)
            return false;

        QRegExp rx(STR("\\s*(\\s?[\\.．。]){10,}\\s*"));
        if (rx.indexIn(pageStart) == -1)
            return true;
    }
    else
    {
        QRegExp ex2(STR("\\s*(Chapter|chapter|CHAPTER)\\s*\\d{1,20}\\s*"));
        if (ex2.indexIn(pageStart) == 0)
        {
            QRegExp rx(STR("\\s*(\\s?[\\.．。]){10,}\\s*"));
            if (rx.indexIn(pageStart) == -1)
                return true;
        }
    }
    return false;
}

bool CheckNewChapter2(const QString& pageStart)
{
    if (CheckNewChapter(pageStart))
        return true;

    QRegExp ex(STR("\\s+(\\d|一|二|三|四|五|六|七|八|九|十|零|百|千){1,20}\\s*(章|节|回|集|卷|部)"));
    if (ex.indexIn(pageStart) != -1)
    {
        if (pageStart.length() > 60)
            return false;

        QRegExp rx(STR("\\s*(\\s?[\\.．。]){10,}\\s*"));
        if (rx.indexIn(pageStart) == -1)
            return true;
    }
    return false;
}

bool CheckNewChapter3(const QString& pageStart)
{
    if (CheckNewChapter2(pageStart))
        return true;

    QRegExp ex(STR("\\s+\\d{1,20}\\s*"));
    if (ex.indexIn(pageStart) != -1)
    {
        if (pageStart.length() > 30)
            return false;

        QRegExp rx(STR("\\s*(\\s?[\\.．。]){10,}\\s*"));
        if (rx.indexIn(pageStart) == -1)
            return true;
    }

    QRegExp ex2(STR("\\s+(\\d|一|二|三|四|五|六|七|八|九|十|零|百|千){1,20}\\s*"));
    if (ex2.indexIn(pageStart) != -1)
    {
        if (pageStart.length() > 30)
            return false;

        QRegExp rx(STR("\\s*(\\s?[\\.．。]){10,}\\s*"));
        if (rx.indexIn(pageStart) == -1)
            return true;
    }
    return false;
};

void CheckNewChapterForPage(const QVector<QString>& page, int pageIndex, 
    QVector<ChapterInfo>& chapters)
{
    for (int iRow=0; iRow < page.size(); iRow++)
    {
        if (CheckNewChapter(page[iRow]))
        {
            ChapterInfo ci;
            ci.byPage = true;
            ci.pageIndex = pageIndex;
            ci.rowIndex = iRow;
            ci.title = page[iRow];
            QString prev, next;
            if (iRow-1 >= 0)
                prev = page[iRow-1];
            if (iRow+1 < page.size()-1)
                next = page[iRow+1];
            ci.context = prev + "\n" + page[iRow] + "\n" + next;
            chapters.push_back(ci);
        }
    }
}

void CheckNewChapterForPage_AtPageStart(const QVector<QString>& page, int pageIndex, 
    QVector<ChapterInfo>& chapters)
{
    int first2rows[2] = {-1, -1};
    for (int iRow=0; iRow < page.size(); iRow++)
    {
        if (!page[iRow].trimmed().isEmpty())
        {
            if (first2rows[0] == -1)
                first2rows[0] = iRow;
            else
            {
                first2rows[1] = iRow;
                break;
            }
        }
    }
    for (int i=0; i<2; i++)
    {
        if (first2rows[i] != -1 && CheckNewChapter(page[first2rows[i]]))
        {
            int iRow = first2rows[i];
            ChapterInfo ci;
            ci.byPage = true;
            ci.pageIndex = pageIndex;
            ci.rowIndex = iRow;
            ci.title = page[iRow];
            QString prev, next;
            if (iRow-1 >= 0)
                prev = page[iRow-1];
            if (iRow+1 < page.size()-1)
                next = page[iRow+1];
            ci.context = prev + "\n" + page[iRow] + "\n" + next;

            chapters.push_back(ci);
            break;
        }
    }
}

void ExtractChaptersByPage(QVector<ChapterInfo>& chapters, const QString& textFullPathFile, 
    const QString& bookTitle, bool chapterAtPageStart)
{
    QFile file(textFullPathFile);
    if (!file.open(QIODevice::ReadOnly))
        return;
    QTextStream readStream(&file);
    readStream.setCodec(CODEC());

    chapters.clear();
    ChapterInfo ci;
    ci.byPage = true;
    ci.title = bookTitle;
    chapters.push_back(ci);

    QString pageStart;
    int pageIndex = 0;
    while(!readStream.atEnd())
    {
        // get one page
        QVector<QString> page;
        if (!pageStart.isEmpty())
            page.push_back(pageStart);

        while (!readStream.atEnd())
        {
            QString line = readStream.readLine();
            int pos = line.indexOf(QChar::fromAscii((char)12));
            if (pos != -1)
            {
                pageStart = line.mid(pos + 1);
                page.push_back(line.mid(0, pos));
                break;
            }
            page.push_back(line);
        }
        if (chapterAtPageStart)
            CheckNewChapterForPage_AtPageStart(page, pageIndex, chapters);
        else
            CheckNewChapterForPage(page, pageIndex, chapters);
        pageIndex ++;
    }
}

void ExtractChapters(QVector<ChapterInfo>& chapters, const QString& textFullPathFile, 
    const QString& bookTitle, int checkMethod)
{
    QFile file(textFullPathFile);
    if (!file.open(QIODevice::ReadOnly))
        return;
    QTextStream readStream(&file);
    readStream.setCodec(CODEC());

    chapters.clear();
    ChapterInfo ci;
    ci.byPage = false;
    ci.title = bookTitle;
    chapters.push_back(ci);

    int rowIndex = 0;
    QString prevLine;
    while(!readStream.atEnd())
    {
        QString line = readStream.readLine();
        if ((checkMethod == 1) ? CheckNewChapter(line) : 
            ((checkMethod == 2) ? CheckNewChapter2(line) : CheckNewChapter3(line)))
        {
            ChapterInfo ci;
            ci.byPage = false;
            ci.pageIndex = -1;
            ci.rowIndex = rowIndex;
            ci.title = line;
            ci.context = prevLine + "\n" + line;
            chapters.push_back(ci);
        }
        rowIndex ++;
        prevLine = line;
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////////

void CreateBookSnbf(const QString& snbfDir, 
    const QString& title, const QString& author, const QString& publisher, const QString& created)
{
    QString abstract = title;
    // Create book.snbf file
    {
        QFile bookSnbf(snbfDir + "book.snbf");
        if (!bookSnbf.open(QIODevice::WriteOnly))
            return;
        QTextStream bookSnbfStream(&bookSnbf);
        bookSnbfStream.setCodec("UTF-8");
        bookSnbfStream << "<book-snbf version=\"1.0\">";
        bookSnbfStream << "<head><name>" << Qt::escape(title) << "</name>";
        bookSnbfStream << "<author>" << Qt::escape(author) << "</author>";
        bookSnbfStream << "<language>ZH-CN</language>";
        bookSnbfStream << "<rights>Default</rights>";
        bookSnbfStream << "<publisher>" << Qt::escape(publisher) << "</publisher>";
        bookSnbfStream << "<generator>BF - Bambook Factory</generator>";
        bookSnbfStream << "<created>" << Qt::escape(created) << "</created>";
        bookSnbfStream << "<abstract>" << Qt::escape(abstract) << "</abstract>";
        bookSnbfStream << "<cover/></head></book-snbf>";
    }
}

int CreateSnbcFilesByPage(const QString& snbcDir, const QString& snbcImagesDir,
    bool addImages, int chapterType, bool insertBlankLineForBlock, 
    const QString& textFullPathFile, const QString& bookTitle,
    const QVector<ChapterInfo>& chaptersForType3)
{
    QFile file(textFullPathFile);
    if (!file.open(QIODevice::ReadOnly))
        return 0;
    QTextStream readStream(&file);
    readStream.setCodec(CODEC());

    int chapters = 0;
    QTextStream* pWriteStream = NULL;
    QFile* pOutFileForChapterType1 = NULL;
    QFile* pOutFileForChapterType2 = NULL;
    QFile* pOutFileForChapterType3 = NULL;

    if (chapterType == 1)
        pWriteStream = pSnbcStart(pOutFileForChapterType1, "1.snbc", snbcDir, bookTitle);

    if (chapterType == 3 && chaptersForType3.size() > 0)
        pWriteStream = pSnbcStart(pOutFileForChapterType3, "1.snbc", snbcDir, chaptersForType3[0].title);

    QString pageStart;
    int pageIndex = 0;
    int currentChapterIndexForType3 = 1;

    while(!readStream.atEnd())
    {
        // get one page
        QVector<QString> page;
        if (!pageStart.isEmpty())
            page.push_back(pageStart);

        while (!readStream.atEnd())
        {
            QString line = readStream.readLine();
            int pos = line.indexOf(QChar::fromAscii((char)12));
            if (pos != -1)
            {
                pageStart = line.mid(pos + 1);
                page.push_back(line.mid(0, pos));
                break;
            }
            page.push_back(line);
        }
        pageIndex ++;

        // add new page.snbc file
        if (chapterType == 2)
            pWriteStream = pSnbcStart(pOutFileForChapterType2, QString("%1.snbc").arg(pageIndex), 
                snbcDir, QCoreApplication::translate("PDFExporter", "Page %1").arg(pageIndex));

        // add new chapter.snbc file
        if (chapterType == 3)
        {
            int currentPageIndex = pageIndex-1;
            int curChapterStartRow(0);
            while (currentChapterIndexForType3 < chaptersForType3.size())
            {
                ChapterInfo ci = chaptersForType3[currentChapterIndexForType3];
                assert(ci.byPage);
                if (ci.pageIndex > currentPageIndex)
                    break;
                assert(ci.pageIndex == currentPageIndex);
                int newChapterStartRow = ci.rowIndex;
                int newChapterEndRow = page.size();
                if (currentChapterIndexForType3 + 1 < chaptersForType3.size())
                {
                    ChapterInfo ciNext = chaptersForType3[currentChapterIndexForType3+1];
                    if (ciNext.pageIndex == currentPageIndex)
                        newChapterEndRow = ciNext.rowIndex;
                }
                // insert page content [curChapterStartRow, newChapterStartRow) to current file
                if (curChapterStartRow < newChapterStartRow)
                {
                    if (curChapterStartRow == 0 && addImages)
                        pAddPageImages(*pWriteStream, snbcImagesDir, pageIndex);

                    QVector<QString> strings(page);
                    strings.remove(newChapterStartRow, page.size() - newChapterStartRow);
                    strings.remove(0, curChapterStartRow);
                    pAddPage(*pWriteStream, strings, insertBlankLineForBlock);
                }
                // create a new snbc file
                {
                    pSnbcEnd(pWriteStream, pOutFileForChapterType3);
                    pWriteStream = pSnbcStart(pOutFileForChapterType3, 
                        QString("%1.snbc").arg(currentChapterIndexForType3+1), snbcDir, 
                        ci.title);
                }
                // insert page content [newChapterStartRow, newChapterEndRow) to new snbc file
                {
                    QVector<QString> strings(page);
                    if (newChapterEndRow < page.size())
                        strings.remove(newChapterEndRow, page.size() - newChapterEndRow);
                    strings.remove(0, newChapterStartRow);

                    if (newChapterEndRow == 0 && addImages)
                        pAddPageImages(*pWriteStream, snbcImagesDir, pageIndex);
                    pAddPage(*pWriteStream, strings, insertBlankLineForBlock);
                }
                curChapterStartRow = newChapterEndRow;
                currentChapterIndexForType3 ++;
            }
            // insert page content [curChapterStartRow, page.size()) to current file
            if (curChapterStartRow == 0)
            {
                if (addImages)
                    pAddPageImages(*pWriteStream, snbcImagesDir, pageIndex);
                pAddPage(*pWriteStream, page, insertBlankLineForBlock);
            }
        }
        else
        {
            if (addImages)
                pAddPageImages(*pWriteStream, snbcImagesDir, pageIndex);
            pAddPage(*pWriteStream, page, insertBlankLineForBlock);
            if (chapterType == 2)
                pSnbcEnd(pWriteStream, pOutFileForChapterType2);
        }

        CHECK_ABORT_RTBOOL;
        OnConverting(PHASE_6_CreatePage, 0, QCoreApplication::translate("PDFExporter", "Generated the content of page %1 successfully.").arg(pageIndex));
    }
    if (chapterType == 1)
    {
        pSnbcEnd(pWriteStream, pOutFileForChapterType1);
        chapters = 1;
    }
    if (chapterType == 2)
        chapters = pageIndex;
    if (chapterType == 3)
    {
        pSnbcEnd(pWriteStream, pOutFileForChapterType3);
        chapters = chaptersForType3.size();
    }
    return chapters;
}

inline void CreateTocSnbf(const QString& snbfDir, int chapters, int chapterType, const QString& title, 
    const QVector<ChapterInfo>& chaptersForType3)
{
    // Create toc.snbf file
    {
        QFile tocSnbf(snbfDir + "toc.snbf");
        if (!tocSnbf.open(QIODevice::WriteOnly))
            return;
        QTextStream tocSnbfStream(&tocSnbf);
        tocSnbfStream.setCodec("UTF-8");
        tocSnbfStream << QString("<toc-snbf><head><chapters>%1</chapters></head><body>").arg(chapters);

        if (chapterType == 1)
        {
            assert(chapters == 1);
            tocSnbfStream << "<chapter src=\"1.snbc\"><![CDATA[" << title
                << "]]></chapter>";
        }
        else if (chapterType == 2)
        {
            // chapter by page
            //assert(chapters == pdfInfo.pages);
            for (int i=0; i<chapters; i++)
            {
                tocSnbfStream << QString("<chapter src=\"%1.snbc\"><![CDATA[").arg(i+1)
                    << QCoreApplication::translate("PDFExporter", "Page %1").arg(i+1)
                    << "]]></chapter>";
            }
        }
        else if (chapterType == 3)
        {
            // chapter by page
            for (int i=0; i<chapters; i++)
            {
                tocSnbfStream << QString("<chapter src=\"%1.snbc\"><![CDATA[").arg(i+1)
                    << chaptersForType3[i].title
                << "]]></chapter>";
            }
        }
        tocSnbfStream << "</body></toc-snbf>";
    }
}

inline void CreateSnbFiles(const QString& snbfDir, const QString& snbcDir, const QString& snbcImagesDir,
    bool addImages, int chapterType, bool insertBlankLineForBlock, const QString& textFullPathFile, 
    const QString& title, const QString& author, const QString& publisher, const QString& created,
    const QVector<ChapterInfo>& chaptersForType3)
{
    CreateBookSnbf(snbfDir, title, author, publisher, created);

    int chapters = CreateSnbcFilesByPage(snbcDir, snbcImagesDir, addImages, 
        chapterType, insertBlankLineForBlock, textFullPathFile, title, chaptersForType3);

    CreateTocSnbf(snbfDir, chapters, chapterType, title, chaptersForType3);
}

inline void qSleep(int ms)
{
    QWaitCondition sleep;
    QMutex temp;
    temp.lock();
    sleep.wait(&temp, ms);
    temp.unlock();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" _declspec(dllexport) 
bool ZConvertPdf(wchar_t* snbRootDir, int bufLen, const wchar_t* xpdfPath0, 
    const wchar_t* pdfFile0, const wchar_t* userDir0, bool isColumnMode, bool extractImages,
    int chapterType, bool* abort, ZOnConvertingCallack cb)
{
    g_pToBeAbort = abort;
    g_onConvertingCB = cb;

    wstring xpdfPath;
    NormalizePath(xpdfPath, xpdfPath0);
    QString userDir = wstr2qstr(userDir0);

    CHECK_ABORT_RTBOOL;

    // C:\Users\xx\BambookFactory\pdf\, to store pdfinfo.txt, pdfrc, pdftxt.
    EnsureExistDir(userDir, "pdf");
    QString pdfDir = userDir + "pdf\\";  

    // C:\Users\xx\BambookFactory\pdf\pdfbookname, to store the snbc and snbf files
    QString pdfFileNameWithoutExt0 = GetFileNameWithoutExt(pdfFile0);
    bool isCKJ = false;
    for (int charIndex = 0; charIndex < pdfFileNameWithoutExt0.length(); charIndex++)
        if (pdfFileNameWithoutExt0[charIndex].unicode() > 255)
        {
            isCKJ = true;
            break;
        }
    QString pdfFileNameWithoutExt = pdfFileNameWithoutExt0;
    if (isCKJ)
        pdfFileNameWithoutExt = "chinesePdfTemp";
    EnsureExistDir(pdfDir, pdfFileNameWithoutExt);

    QString thisPdfDir = pdfDir + pdfFileNameWithoutExt + "\\";
    CleanDirectory(QDir(thisPdfDir));

    // C:\Users\xx\BambookFactory\pdf\pdfbooknameTemp
    EnsureExistDir(pdfDir, pdfFileNameWithoutExt + "Temp");
    QString thisPdfTempDir = pdfDir + pdfFileNameWithoutExt + "Temp\\";
    CleanDirectory(QDir(thisPdfTempDir));

    //C:\Users\xx\BambookFactory\pdf\pdfbookname\snbc\,
    EnsureExistDir(thisPdfDir, "snbf");
    EnsureExistDir(thisPdfDir, "snbc");
    EnsureExistDir(thisPdfDir, "snbf");
    QString snbcDir = thisPdfDir + "snbc\\";
    QString snbfDir = thisPdfDir + "snbf\\";

    //C:\Users\xx\BambookFactory\pdf\pdfbookname\snbc\images\, to store images
    EnsureExistDir(snbcDir, "images");
    QString snbcImagesDir = snbcDir + "images\\";

    CHECK_ABORT_RTBOOL;
    OnConverting(PHASE_1_CreateFolders, 0, QCoreApplication::translate("PDFExporter", "Generated temp folders successfully."));

    // create rc file to user dir
    wstring wsPdfDir;
    CreateXpdfRcFile(xpdfPath, qstr2wstr(wsPdfDir, pdfDir).c_str());

    CHECK_ABORT_RTBOOL;
    OnConverting(PHASE_2_CreateConfig, 0, QCoreApplication::translate("PDFExporter", "Created configurations successfully."));

    // extract pdfout.txt to user dir
    CHECK_ABORT_RTBOOL;
    OnConverting(PHASE_3_ExtractText, 0, QCoreApplication::translate("PDFExporter", "Extracting text content from PDF file..."));

    wstring wsThisPdfTempDir;
    if (!ExtractTxtFromPdf(xpdfPath, pdfFile0, wsPdfDir.c_str(), 
        qstr2wstr(wsThisPdfTempDir, thisPdfTempDir).c_str(), isColumnMode))
        return false;

    CHECK_ABORT_RTBOOL;
    OnConverting(PHASE_3_ExtractText, 0, QCoreApplication::translate("PDFExporter", "Extracted text content from PDF file successfully."));

    // get pdf info
    PdfInfo pdfInfo;
    if (!GetPdfInfo(pdfInfo, xpdfPath, pdfFile0))
        return false;

    CHECK_ABORT_RTBOOL;
    OnConverting(PHASE_4_ReadHeadInfo, 0, QCoreApplication::translate("PDFExporter", "Extracted book information from PDF file successfully."));

    // get images
    if (extractImages)
    {
        OnConverting(PHASE_5_ExtractImages, 0, QCoreApplication::translate("PDFExporter", "Extracting images content from PDF file..."));
        if (!ExtractPdfImages2(pdfInfo.pages, xpdfPath, pdfFile0, snbcImagesDir))
            return false;
        CHECK_ABORT_RTBOOL;
        OnConverting(PHASE_5_ExtractImages, 0, QCoreApplication::translate("PDFExporter", "Extracted all images from PDF file successfully."));
    }

    // get title
    QString title = wstr2qstr(pdfInfo.title.c_str());
    if (title.trimmed().isEmpty())
        title = pdfFileNameWithoutExt0;

    // create book.snbf file
    QString author = wstr2qstr(pdfInfo.author.c_str());
    QString publisher = wstr2qstr(pdfInfo.producer.c_str());
    QString created = wstr2qstr(pdfInfo.creationDate.c_str());
    QString textFullPathFile = thisPdfTempDir + "pdfout.txt";
    bool insertBlankLineForBlock = isColumnMode;
    
    // extract chapters
    if (chapterType == 3)
    {
        ExtractChaptersByPage(g_chapters1, textFullPathFile, title, true);
        ExtractChaptersByPage(g_chapters2, textFullPathFile, title, false);
    }
    g_useLeft = true;
    OnConverting(PHASE_7_ChooseChapters);
    
    qSleep(200);

    g_mutex.lock();
    g_mutex.unlock();

    // create snb folder and files
    CreateSnbFiles(snbfDir, snbcDir, snbcImagesDir,
        extractImages, chapterType, insertBlankLineForBlock, 
        textFullPathFile, 
        title, author, publisher, created, g_useLeft ? g_chapters1 : g_chapters2);

    // return the snbRootDir (for packing)
    assert(thisPdfDir.length() <  bufLen);
    wstring wsThisPdfDir;
    _tcscpy_s(snbRootDir, bufLen, qstr2wstr(wsThisPdfDir, thisPdfDir).c_str());

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CreateSnbcFiles(const QString& snbcDir, const QString& textFullPathFile, 
    const QString& bookTitle, const QVector<ChapterInfo>& chapters)
{
    QFile file(textFullPathFile);
    if (!file.open(QIODevice::ReadOnly))
        return;
    QTextStream readStream(&file);
    readStream.setCodec(CODEC());

    QTextStream* pWriteStream = NULL;
    QFile* pOutFile = NULL;

    if (chapters.size() > 0)
        pWriteStream = pSnbcStart(pOutFile, "1.snbc", snbcDir, chapters[0].title);

    int currentChapterIndex = 1;
    int currentLineIndex = 0;
    int COUNT_PER_PAGE = 100;//new
    QVector<QString> page;//new

    while(!readStream.atEnd())
    {
        QString line = readStream.readLine();
        if (currentChapterIndex < chapters.size())
        {
            ChapterInfo ci = chapters[currentChapterIndex];
            if (ci.rowIndex == currentLineIndex)
            {
                pAddPage((*pWriteStream), page, false);//new
                page.clear();//new

                pSnbcEnd(pWriteStream, pOutFile);
                pWriteStream = pSnbcStart(pOutFile, 
                    QString("%1.snbc").arg(currentChapterIndex+1), snbcDir, 
                    ci.title);
                currentChapterIndex ++;
            }
        }
        currentLineIndex++;

        page.push_back(line);//new
        if (page.count() >= COUNT_PER_PAGE)//new
        {//new
            pAddPage((*pWriteStream), page, false);//new
            page.clear();//new
        }//new

        // insert line to current file
        //(*pWriteStream) << line << "\n";

        CHECK_ABORT;
        OnConverting(PHASE_6_CreatePage, 0, QCoreApplication::translate("PDFExporter", "Generated the content of line %1 successfully.").arg(currentLineIndex));
    }

    pAddPage((*pWriteStream), page, false);//new
    page.clear();//new
    pSnbcEnd(pWriteStream, pOutFile);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

inline void CreateSnbFiles(const QString& snbfDir, const QString& snbcDir, const QString& textFullPathFile, 
    const QString& title, const QString& author, const QString& publisher, const QString& created,
    const QVector<ChapterInfo>& chapters)
{
    CreateBookSnbf(snbfDir, title, author, publisher, created);

    CreateSnbcFiles(snbcDir, textFullPathFile, title, chapters);

    CreateTocSnbf(snbfDir, chapters.size(), 3, title, chapters);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" PDF_API
    bool ZConvertTxt(wchar_t* snbRootDir, int bufLen, 
    const wchar_t* txtFile0, const wchar_t* userDir0, bool* abort, ZOnConvertingCallack cb)
{
    g_pToBeAbort = abort;
    g_onConvertingCB = cb;

    QString userDir = wstr2qstr(userDir0);

    CHECK_ABORT_RTBOOL;

    // C:\Users\xx\BambookFactory\txt
    EnsureExistDir(userDir, "txt");
    QString txtDir = userDir + "txt\\";  

    // C:\Users\xx\BambookFactory\txt\txtbookname, to store the snbc and snbf files
    QString txtFileNameWithoutExt = GetFileNameWithoutExt(txtFile0);
    EnsureExistDir(txtDir, txtFileNameWithoutExt);

    QString thisTxtDir = txtDir + txtFileNameWithoutExt + "\\";
    CleanDirectory(QDir(thisTxtDir));

    //C:\Users\xx\BambookFactory\txt\txtbookname\snbc\,
    EnsureExistDir(thisTxtDir, "snbf");
    EnsureExistDir(thisTxtDir, "snbc");
    QString snbcDir = thisTxtDir + "snbc\\";
    QString snbfDir = thisTxtDir + "snbf\\";

    CHECK_ABORT_RTBOOL;
    OnConverting(PHASE_1_CreateFolders, 0, QCoreApplication::translate("PDFExporter", "Generated temp folders successfully."));

    // get title
    QString title = txtFileNameWithoutExt;
    QString textFullPathFile = wstr2qstr(txtFile0);

    // extract chapters
    CHECK_ABORT_RTBOOL;
    OnConverting(PHASE_1_CreateFolders, 0, QCoreApplication::translate("PDFExporter", "Extracting chapters information."));

    g_useLeft = true;
    g_retry = false;
    ExtractChapters(g_chapters1, textFullPathFile, title, 1);
    OnConverting(PHASE_7_ChooseChapters);
    qSleep(200);
    g_mutex.lock();
    g_mutex.unlock();

    if (g_retry)
    {
        g_retry = false;
        g_chapters1.clear();
        ExtractChapters(g_chapters1, textFullPathFile, title, 2);
        OnConverting(PHASE_7_ChooseChapters);
        qSleep(200);
        g_mutex.lock();
        g_mutex.unlock();

        if (g_retry)
        {
            g_retry = true; // to prevent the "retry" button
            g_chapters1.clear();
            ExtractChapters(g_chapters1, textFullPathFile, title, 3);
            OnConverting(PHASE_7_ChooseChapters);
            qSleep(200);
            g_mutex.lock();
            g_mutex.unlock();
        }
    }

    // create snb folder and files
    CreateSnbFiles(snbfDir, snbcDir, textFullPathFile, 
        title, "", "", "", g_chapters1);

    // return the snbRootDir (for packing)
    assert(thisTxtDir.length() <  bufLen);
    wstring wsThisTxtDir;
    _tcscpy_s(snbRootDir, bufLen, qstr2wstr(wsThisTxtDir, thisTxtDir).c_str());

    return true;
}
