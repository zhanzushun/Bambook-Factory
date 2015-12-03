#include "stdafx.h"
#include "readerWindow.hxx"
#include "zdisplay.h"

static wstring g_snbDir;
static wstring g_snbTempDir;
static QString g_homeDir;
static wstring g_homeDir2;

void ensureExistDir(const QString& dir, const QString& subdir)
{
    QDir d(dir);
    if (!d.exists(subdir))
        d.mkdir(subdir);
}

QString getHomeDir()
{
    if (g_homeDir.isEmpty())
    {
        g_homeDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
        g_homeDir.replace("/", "\\");
        ensureExistDir(g_homeDir, "BambookFactory");
        g_homeDir = g_homeDir + "\\BambookFactory";

        ensureExistDir(g_homeDir, "temp");
        ensureExistDir(g_homeDir, "snb");
        ensureExistDir(g_homeDir, "pdf");

        g_homeDir += "\\";

        //{
        //    QString strSourceFiles = QCoreApplication::applicationDirPath();
        //    strSourceFiles += "/snb/*.*";
        //    QString strSnbDir = getHomeDir() + "snb\\";
        //    strSourceFiles.replace('/','\\');
        //    strSnbDir.replace('/','\\');

        //    QProcess* p = new QProcess(g_mainwindow);
        //    QString str = QString("xcopy.exe \"%1\" \"%2\"").arg(strSourceFiles).arg(strSnbDir);
        //    p->start(str);
        //    
        //    QWaitCondition sleep;
        //    QMutex temp;
        //    temp.lock();
        //    sleep.wait(&temp,500);
        //}
    }
    return g_homeDir;
}

const wchar_t * ZGetHomeDir()
{
    if (g_homeDir2.empty())
        qstr2wstr(g_homeDir2, getHomeDir());
    return g_homeDir2.c_str();
}

const wchar_t * ZGetSnbDir()
{
    if (g_snbDir.empty())
        qstr2wstr(g_snbDir, getHomeDir() + "snb\\");
    return g_snbDir.c_str();
}

const wchar_t * ZGetSnbTempDir()
{
    if (g_snbTempDir.empty())
        qstr2wstr(g_snbTempDir, getHomeDir() + "temp\\");
    return g_snbTempDir.c_str();
}

ZBookInfo::ZBookInfo()
{
    cover = L":/images/defaultCover.png";
}

bool readXmlText(QString& data, const QDomNode &nd, const QString& prop) 
{
    if (nd.nodeName() == prop)
    {
        if (nd.childNodes().length() != 0)
        {
            if (nd.childNodes().at(0).nodeType() == QDomNode::CDATASectionNode)
            {
                QDomCDATASection* dataNode = (QDomCDATASection*) & (nd.childNodes().at(0));
                data = dataNode->data();
                return true;
            }
            else if (nd.childNodes().at(0).nodeType() == QDomNode::TextNode)
            {
                QDomText * textNode = (QDomText*) & (nd.childNodes().at(0));
                data = textNode->data();
                return true;
            }
        }
    }
    return false;
}

bool readBookHeader(ZBookInfo & bookInfo, const QString& bookSnbf, const QString& tocSnbf)
{
    QFile file(bookSnbf);
    if (!file.open(QIODevice::ReadOnly)) 
    {
        Q_ASSERT(false);
        return false;
    }
    QDomDocument doc;
    if (!doc.setContent(&file))
        return false;
    if (doc.documentElement().nodeName() != "book-snbf")
        return false;
    if (doc.documentElement().childNodes().length() == 0)
        return false;
    QDomNode head = doc.documentElement().childNodes().at(0);
    if (head.nodeName() != "head")
        return false;
    
    QString name, author, language, rights, publisher, generator, abstract, created, cover;
    for(int i=0; i<(int)head.childNodes().length(); i++)
    {
        QDomNode child = head.childNodes().at(i);
        readXmlText(name, child, "name");
        readXmlText(author, child, "author");
        readXmlText(language, child, "language");
        readXmlText(rights, child, "rights");
        readXmlText(publisher, child, "publisher");
        readXmlText(generator, child, "generator");
        readXmlText(abstract, child, "abstract");
        readXmlText(created, child, "created");
        readXmlText(cover, child, "cover");
    }
    qstr2wstr(bookInfo.name, name);
    qstr2wstr(bookInfo.author, author);
    qstr2wstr(bookInfo.language, language);
    qstr2wstr(bookInfo.rights, rights);
    qstr2wstr(bookInfo.publisher, publisher);
    qstr2wstr(bookInfo.generator, generator);
    qstr2wstr(bookInfo.abstract, abstract);
    qstr2wstr(bookInfo.created, created);
    qstr2wstr(bookInfo.cover, cover);

    QFile file2(tocSnbf);
    if (!file2.open(QIODevice::ReadOnly)) 
    {
        Q_ASSERT(false);
        return false;
    }
    QDomDocument doc2;
    if (!doc2.setContent(&file2))
        return false;
    if (doc2.documentElement().nodeName() != "toc-snbf")
        return false;
    if (doc2.documentElement().childNodes().length() < 2)
        return false;
    QDomNode head2 = doc2.documentElement().childNodes().at(0);
    if (head2.nodeName() != "head")
        return false;
    QDomNode body2 = doc2.documentElement().childNodes().at(1);
    if (body2.nodeName() != "body")
        return false;

    QString chapters;
    for(int i=0; i<(int)head2.childNodes().length(); i++)
    {
        QDomNode child = head2.childNodes().at(i);
        readXmlText(chapters, child, "chapters");
    }
    for(int i=0; i<(int)body2.childNodes().length(); i++)
    {
        QDomNode chapter = body2.childNodes().at(i);
        QString src, title;
        readXmlText(title, chapter, "chapter");
        QDomElement* elm = (QDomElement*) & chapter;
        src = elm->attribute("src");
        wstring wsrc, wtitle;
        qstr2wstr(wsrc, src);
        qstr2wstr(wtitle, title);
        bookInfo.chapters.push_back(make_pair(wsrc, wtitle));
    }
    return true;
}

static set<QString> g_openedFiles;

bool extractFileFromSnb(QString& targetFile, const QString& snbSourceFile, 
    const wchar_t* internalRelativePathFile, bool forceReflesh = false)
{
    QString snbFullPathFile = snbSourceFile;
    QFileInfo fi(snbFullPathFile);
    if (fi.isRelative())
        snbFullPathFile = wstr2qstr(ZGetSnbDir()) + snbSourceFile;
    QFileInfo fi2(snbFullPathFile);
    if (!fi2.exists())
        return false;

    wstring wsnbFullPathFile;
    qstr2wstr(wsnbFullPathFile, snbFullPathFile);
    if (!verifySnb(wsnbFullPathFile.c_str()))
        return false;

    QString ext = ".snb";
    QString fileName = fi2.fileName();
    QString snbFileName = fileName.mid(0, fileName.length() - ext.length());
    QString tempDir = wstr2qstr(ZGetSnbTempDir());
    QString tempSnbDir = tempDir + snbFileName + "\\";

    ensureExistDir(tempDir, snbFileName);

    QString relativeFile = wstr2qstr(internalRelativePathFile); // must use "/" as dir separator
    QStringList list = relativeFile.split('/');
    targetFile = tempSnbDir + relativeFile;
    targetFile.replace('/', '\\');

    QString dir = tempSnbDir;
    for (int i=0; i<list.count()-1; i++) // last one is file name, not dir
    {
        ensureExistDir(dir, list[i]);
        dir = dir + list[i] + "\\";
    }

    //if (!forceReflesh && g_openedFiles.find(targetFile) != g_openedFiles.end())
    //{
    //    QFileInfo fi(targetFile);
    //    targetFile = fi.absoluteFilePath();
    //    return true;
    //}

    wstring buf;
    if (unpackSnb(wsnbFullPathFile.c_str(), internalRelativePathFile, 
        qstr2wstr(buf, targetFile).c_str()))
    {
        g_openedFiles.insert(targetFile);
        QFileInfo fi(targetFile);
        targetFile = fi.absoluteFilePath();
        return true;
    }
    return false;
}

bool extractImageFromSnb(QString& targetFile, const QString& snbSourceFile, 
    const wchar_t* imageSrc)
{
    wstring image;
    if (*imageSrc == L'/')
        image = wstring(L"snbc/images") + imageSrc;
    else
        image = wstring(L"snbc/images/") + imageSrc;
    return extractFileFromSnb(targetFile, snbSourceFile, image.c_str());
}

bool ZGetBookInfo(const wchar_t* snbFile, ZBookInfo& bookInfo)
{
    QString snbFullPathFile = wstr2qstr(snbFile);
    QFileInfo fi(snbFullPathFile);
    if (fi.isRelative())
        snbFullPathFile = wstr2qstr(ZGetSnbDir()) + snbFullPathFile;
    QFileInfo fi2(snbFullPathFile);
    if (!fi2.exists())
        return false;
    qstr2wstr(bookInfo.id, fi2.fileName());

    QString sBookFile, sTocFile, qsnbFile(wstr2qstr(snbFile));
    if (!extractFileFromSnb(sBookFile, qsnbFile, L"snbf/book.snbf"))
        return false;
    if (!extractFileFromSnb(sTocFile, qsnbFile, L"snbf/toc.snbf"))
        return false;
    QFileInfo fi3(sBookFile);
    QFileInfo fi4(sTocFile);
    Q_ASSERT(fi3.exists() && fi4.exists());
    if (!fi3.exists() || !fi4.exists())
        return false;
    if (!readBookHeader(bookInfo, sBookFile, sTocFile))
        return false;
    if (!bookInfo.cover.empty())
    {
        QString imgFile;
        if (extractImageFromSnb(imgFile, qsnbFile, bookInfo.cover.c_str()))
            qstr2wstr(bookInfo.cover, imgFile);
    }
    else
        bookInfo.cover = L":/images/defaultCover.png";
    return true;
}

static ReaderWindow* g_readerWindow = NULL;

bool ZReadBook(const wchar_t* snbFile, bool forceReflash)
{
    if (g_readerWindow == NULL)
        g_readerWindow = new ReaderWindow(g_mainwindow);
    g_readerWindow->readBook(snbFile, forceReflash);
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

ReaderWindow::ReaderWindow(QWidget *parent)
    : QMainWindow(parent), m_pChapterStream(NULL), m_pChapterFile(NULL)
{
    setupUi(this);
    textBrowser->setOpenLinks(false);
    textBrowser_2->setOpenLinks(false);
    connect(textBrowser, SIGNAL(anchorClicked(const QUrl&)), this, SLOT(leftAnchorClicked(const QUrl&)));
    connect(textBrowser_2, SIGNAL(anchorClicked(const QUrl&)), this, SLOT(rightAnchorClicked(const QUrl&)));
    m_enterTextBlock = false;
    m_endTextBlock = false;
}

ReaderWindow::~ReaderWindow()
{
    if (m_pChapterStream != NULL)
    {
        delete m_pChapterStream;
        m_pChapterStream = NULL;
    }
    if (m_pChapterFile != NULL)
    {
        delete m_pChapterFile;
        m_pChapterFile = NULL;
    }
}

bool replaceText(QTextBrowser * textBrowser, const QString& mark, const wstring& value)
{
    textBrowser->moveCursor(QTextCursor::Start);
    if (!textBrowser->find(mark))
        return false;
    QTextCursor cursor = textBrowser->textCursor();
    cursor.removeSelectedText();
    if (value.empty())
        return true;
    cursor.insertText(wstr2qstr(value.c_str()));
    return true;
}

bool ReaderWindow::readBook(const wchar_t* snb, bool forceReflash)
{
    QString qsnbbook = wstr2qstr(snb);
    if (m_snbbook == qsnbbook && !forceReflash)
    {
        show();
        return true;
    }
    m_snbbook = qsnbbook;
    if (m_pChapterStream != NULL)
    {
        delete m_pChapterStream;
        m_pChapterStream = NULL;
    }
    if (m_pChapterFile != NULL)
    {
        delete m_pChapterFile;
        m_pChapterFile = NULL;
    }

    ZBookInfo bi;
    if (!ZGetBookInfo(snb, bi))
        return false;
    setWindowTitle(m_snbbook);
    textBrowser->setSource(QUrl(QString("file:///") + QCoreApplication::applicationDirPath() + "/book.htm"));

    replaceText(textBrowser, "{bookname}", bi.name);
    replaceText(textBrowser, "{author}", bi.author);
    replaceText(textBrowser, "{language}", bi.language);
    replaceText(textBrowser, "{rights}", bi.rights);
    replaceText(textBrowser, "{publisher}", bi.publisher);
    replaceText(textBrowser, "{generator}", bi.generator);
    replaceText(textBrowser, "{created}", bi.created);
    replaceText(textBrowser, "{abstract}", bi.abstract);
    
    textBrowser->moveCursor(QTextCursor::Start);
    if (textBrowser->find("{cover}"))
    {
        QTextCursor cursor = textBrowser->textCursor();
        cursor.removeSelectedText();
        if (!bi.cover.empty())
        {
            QImage img(wstr2qstr(bi.cover.c_str()));
            cursor.insertImage(img);
        }
    }
    if (textBrowser->find("{chapters}"))
    {
        QTextCursor cursor = textBrowser->textCursor();
        list<pair<wstring, wstring> >::const_iterator it = bi.chapters.begin();
        for (; it!=bi.chapters.end(); ++it)
        {
            cursor.insertBlock();
            cursor.insertHtml(QString("<span style=\"font-size:12pt\"><a href=\"file:///%1\">%2</a></span>").
                arg(wstr2qstr(it->first.c_str())).
                arg(wstr2qstr(it->second.c_str()))
                );
        }
    }
    textBrowser->moveCursor(QTextCursor::Start);
    textBrowser_2->clear();
    show();
    repaint();
    if (bi.chapters.size() > 0)
    {
        // goto the first chapter
        leftAnchorClicked(QUrl(QString("file:///") + wstr2qstr(bi.chapters.begin()->first.c_str())));
        textBrowser->moveCursor(QTextCursor::Start);
    }
    return true;
}

void handleLine(QTextCursor& textCursor, QString& line, bool& enterBlock, bool& endBlock, 
    const QString& blockEnterId, const QString& blockEndId, QString& image, const QString& snbbook)
{
    if (!enterBlock)
    {
        int pos = line.indexOf(blockEnterId);
        if (pos != -1)
        {
            enterBlock = true;
            line = line.mid(pos + blockEnterId.length());
            if (line.trimmed().isEmpty())
                return;
        }
    }
    if (enterBlock)
    {
        QString str;
        int pos = line.indexOf(blockEndId);
        if (pos != -1)
        {
            enterBlock = false;
            endBlock = true;
            if (line.trimmed() == blockEndId)
                return;
            str = line.mid(0, pos);
            line = line.mid(pos + blockEndId.length());
        }
        else
        {
            str = line;
            line = "";
        }

        if (str.contains("]]>") || str.contains("<![CDATA["))
        {
            QString tstr = str;
            tstr.remove("<![CDATA[");
            tstr.remove("]]>");
            if (tstr.trimmed().isEmpty())
                return;
        }
        str.remove("<![CDATA[");
        str.remove("]]>");

        if (blockEnterId == "<img")
        {
            image += str;
            if (endBlock)
            {
                int pos2 = image.indexOf(">"); 
                if (pos2 != -1)
                    image = image.mid(pos2 + 1);
                if (!image.isEmpty())
                {
                    QString imgFile; wstring wimg;
                    if (extractImageFromSnb(imgFile, snbbook, qstr2wstr(wimg, image).c_str()))
                    {
                        textCursor.insertBlock();
                        textCursor.insertImage(QImage(imgFile));
                    }
                }
            }
        }
        else if (blockEnterId == "<title>" && !str.isEmpty())
        {
            textCursor.insertBlock();
            QTextCharFormat fmt;
            fmt.setFontPointSize(15);
            fmt.setFontWeight(QFont::Bold);
            textCursor.setBlockCharFormat(fmt);
            textCursor.insertText(str);
            textCursor.insertBlock();
        }
        else if (blockEnterId == "<text>" && !str.isEmpty())
        {
            textCursor.insertBlock();
            QTextCharFormat fmt;
            fmt.setFontPointSize(12);
            textCursor.setBlockCharFormat(fmt);
            textCursor.insertText(str);
        }
    }
}

void ReaderWindow::readPage()
{
    bool enterTitleBlock = false;
    bool endTitleBlock = false;
    bool enterImageBlock = false;
    bool endImageBlock = false;
    QString image;
    QString line;

    int PAGE_HEIGHT = 10000;
    while(!m_pChapterStream->atEnd())
    {
        QString newline = m_pChapterStream->readLine();
        line += newline;

        while (true)
        {
            handleLine(textBrowser_2->textCursor(), line, enterTitleBlock, endTitleBlock, 
                "<title>", "</title>", image, m_snbbook);
            handleLine(textBrowser_2->textCursor(), line, m_enterTextBlock, m_endTextBlock, 
                "<text>", "</text>", image, m_snbbook);
            handleLine(textBrowser_2->textCursor(), line, enterImageBlock, endImageBlock, 
                "<img", "</img>", image, m_snbbook);
            if (enterTitleBlock && line.indexOf("</title>") == -1)
                break;
            if (m_enterTextBlock && line.indexOf("</text>") == -1)
                break;
            if (enterImageBlock && line.indexOf("</img>") == -1)
                break;
            if (!enterTitleBlock && !m_enterTextBlock && !enterImageBlock)
            {
                QRegExp rx("(<title>|<text>|<img)");
                if (rx.indexIn(line) == -1)
                    break;
            }
        }
        if (textBrowser_2->document()->size().height() - m_currentHeight > PAGE_HEIGHT)
        {
            m_currentHeight += PAGE_HEIGHT;
            break;
        }
    }
    if (!m_pChapterStream->atEnd())
    {
        textBrowser_2->textCursor().insertBlock();
        textBrowser_2->textCursor().insertBlock();
        textBrowser_2->textCursor().insertHtml(
            QString("<A style=\"font-size:12pt\" href=\"file:///readmore\">%1</A>").arg(tr("Read more")));
    }
}

void ReaderWindow::leftAnchorClicked(const QUrl& url)
{
    QString s = "snbc/";
    QString temp = "file:///";
    s += url.toString().mid(temp.length());

    textBrowser_2->clear();

    m_currentHeight = 0;
    m_enterTextBlock = false;
    m_endTextBlock = false;
    if (m_pChapterStream != NULL)
    {
        delete m_pChapterStream;
        m_pChapterStream = NULL;
    }
    if (m_pChapterFile != NULL)
    {
        delete m_pChapterFile;
        m_pChapterFile = NULL;
    }

    QString targetFile;
    wstring ws;
    if (extractFileFromSnb(targetFile, m_snbbook, qstr2wstr(ws, s).c_str(), true))
    {
        m_pChapterFile = new QFile(targetFile);
        if (m_pChapterFile->open(QIODevice::ReadOnly))
        {
            m_pChapterStream = new QTextStream(m_pChapterFile);
            m_pChapterStream->setCodec("UTF-8");
            readPage();
        }
    }
}

void ReaderWindow::rightAnchorClicked(const QUrl& url)
{
    QString temp = "file:///";
    temp = url.toString().mid(temp.length());
    Q_ASSERT(temp == "readmore");

    bool bClearPrevious = false;
    if (bClearPrevious)
    {
        textBrowser_2->clear();
        m_currentHeight = 0;
    }
    if (m_pChapterStream == NULL)
        return;
    readPage();
}

