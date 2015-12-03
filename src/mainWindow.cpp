#include "stdafx.h"
#include "mainwindow.hxx"
#include "zdisplay.h"
#include "pdfMainWindow.hxx"
#include "txtMainWindow.hxx"

QWidget* g_mainwindow = NULL;

void transferingFromDeviceCB(int status, int progress, const wchar_t* bookId, void* cbParam)
{
    Q_ASSERT(g_mainwindow != NULL);
    if (g_mainwindow == NULL)
        return;
    ((MainWindow*)g_mainwindow)->onTransferingFromDevice(status, progress, bookId, cbParam);
}

void transferingToDeviceCB(int status, int progress, const wchar_t* snbFile, void* cbParam)
{
    Q_ASSERT(g_mainwindow != NULL);
    if (g_mainwindow == NULL)
        return;
    ((MainWindow*)g_mainwindow)->onTransferingToDevice(status, progress, snbFile, cbParam);
}

void onConnectedCB(const wchar_t* sn, const wchar_t* fv, int totalSpaceInKb, int freeSpaceInKb, 
    const list<ZDeviceBookInfo>& booklist)
{
    Q_ASSERT(g_mainwindow != NULL);
    if (g_mainwindow == NULL)
        return;
    ((MainWindow*)g_mainwindow)->onDeviceConnected(sn, fv, totalSpaceInKb, freeSpaceInKb, booklist);
}

void onBooklistChangedCB(int totalSpaceInKb, int freeSpaceInKb, 
    const list<ZDeviceBookInfo>& booklist)
{
    Q_ASSERT(g_mainwindow != NULL);
    if (g_mainwindow == NULL)
        return;
    ((MainWindow*)g_mainwindow)->onDeviceUpdated(totalSpaceInKb, freeSpaceInKb, booklist);
}

void onDisConnectedCB()
{
    Q_ASSERT(g_mainwindow != NULL);
    if (g_mainwindow == NULL)
        return;
    ((MainWindow*)g_mainwindow)->onDeviceDisconnected();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

SnbLibrary::SnbLibrary(QObject *parent): QAbstractListModel(parent)
{
}

int SnbLibrary::rowCount(const QModelIndex & /* parent */) const
{
    return m_bookList.size();
}

QVariant SnbLibrary::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= m_bookList.count() || index.row() < 0)
        return QVariant();

    const ZBookInfo& book = m_bookList.at(index.row());

    if (role == Qt::DisplayRole)
        return QString("%1\n(%2)").arg(wstr2qstr(book.name.c_str())).arg(wstr2qstr(book.id.c_str()));

    else if (role == Qt::ToolTipRole)
        return QString(QCoreApplication::translate("ToolTipForSnb", 
            "file:%1\nname:%2\nauthor:%3\ndescription:%4")).
            arg(wstr2qstr(book.id.c_str())).arg(wstr2qstr(book.name.c_str())).
            arg(wstr2qstr(book.author.c_str())).arg(wstr2qstr(book.abstract.c_str()));

    else if (role == Qt::DecorationRole)
        return QImage(wstr2qstr(book.cover.c_str())).scaledToHeight(128);

    else if (role == Qt::UserRole)
        return wstr2qstr(book.id.c_str());

    return QVariant();
}

Qt::ItemFlags SnbLibrary::flags(const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QAbstractListModel::flags(index);
    if (index.isValid())
        return Qt::ItemIsDragEnabled | defaultFlags;
    return defaultFlags;
}

QMimeData* SnbLibrary::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    foreach (QModelIndex index, indexes) 
    {
        if (index.isValid()) {
            stream << data(index, Qt::UserRole).value<QString>();
        }
    }
    mimeData->setData("application/snb.id", encodedData);
    return mimeData;
}

QStringList SnbLibrary::mimeTypes() const
{
    QStringList types;
    types << "application/snb.id";
    return types;
}

void SnbLibrary::beginReset()
{
    beginResetModel();
}

void SnbLibrary::endReset()
{
    endResetModel();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

MainWindow::MainWindow(QWidget *parent): QMainWindow(parent)
{
    m_translater = new QTranslator(this);
    m_translater->load("bf_zh", QCoreApplication::applicationDirPath());
    qApp->installTranslator(m_translater);
    m_lang = "zh";
    m_progressBar = NULL;
    m_isPendingRead = false;

    g_mainwindow = this;
    setupUi(this);
    setupUi2();
    updateLeftLibrary();

    connect(actionConnect, SIGNAL(triggered()), this, SLOT(connectBB()));
    connect(actionCopyToBambook, SIGNAL(triggered()), this, SLOT(copyToBB()));
    connect(actionCopyFromBambook, SIGNAL(triggered()), this, SLOT(copyFromBB()));
    connect(actionReadTheBook, SIGNAL(triggered()), this, SLOT(readBook()));
    connect(actionDeleteTheBook, SIGNAL(triggered()), this, SLOT(deleteBook()));
    connect(actionOptions, SIGNAL(triggered()), this, SLOT(options()));
    connect(actionOpenTheFolder, SIGNAL(triggered()), this, SLOT(openFolder()));
    connect(actionImportPDF, SIGNAL(triggered()), this, SLOT(importPDF()));
    connect(actionTxtImport, SIGNAL(triggered()), this, SLOT(importTxt()));

    connect(listViewLeft->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, 
        const QItemSelection&)), this, SLOT(leftSelectionChanged()));
    connect(listViewRight->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&,
        const QItemSelection&)), this, SLOT(rightSelectionChanged()));
    connect(listViewLeft, SIGNAL(doubleClicked(const QModelIndex &)), this, 
        SLOT(doubleClickedLeft(const QModelIndex &)));
    connect(btnRefreshLeft, SIGNAL(clicked(bool)), this, SLOT(btnRefreshLeftClicked()));

    ZStartConnectThread(onConnectedCB, onBooklistChangedCB, onDisConnectedCB);
    updateActions();
}

MainWindow::~MainWindow()
{
    ZStopConnectThread(true);
    g_mainwindow = NULL;
}

void MainWindow::closeEvent(QCloseEvent *)
{
    //QMessageBox mb;
    //mb.setText(tr("Good Bye."));
    //mb.setWindowTitle(tr("Good Bye", "Window Title"));
    //mb.show();
    //mb.repaint();
    ZStopConnectThread(true);
    QCoreApplication::quit();
}

void MainWindow::setupUi2()
{
    m_listViewStyle = QString::fromUtf8(
        "QListView::item {\n"
        "     margin: 2px;\n"
        "     border: 1px solid;\n"
        "     border-radius: 5px;\n"
        "	  border-color: rgb(170, 170, 255);\n"
        "}\n"
        "QListView::item:selected:active {\n"
        "     background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #6a6ea9, stop: 1 #888dd9);\n"
        " }\n"
        "QListView::item:hover {\n"
        "     background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #FAFBFE, stop: 1 #DCDEF1);\n"
        "}"
        "QListView {"
            //"background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1, stop:0 rgba(215, 235, 255, 255), stop:1 rgba(255, 255, 255, 255));"
        "}");

    splitter = new QSplitter;
    verticalLayout->addWidget(splitter);

    frameLeft = new QFrame;
    verticalLayoutLeft = new QVBoxLayout(frameLeft);

    horizontalLayoutLeft = new QHBoxLayout();
    horizontalLayoutLeft->setObjectName(QString::fromUtf8("horizontalLayoutLeft"));
    btnRefreshLeft = new QPushButton(frameLeft);
    btnRefreshLeft->setObjectName(QString::fromUtf8("btnRefreshLeft"));
    btnRefreshLeft->setStyleSheet(QString::fromUtf8("QPushButton{font-size:12px;}"));
    horizontalLayoutLeft->addWidget(btnRefreshLeft);
    labelLeft = new QLabel(frameLeft);
    labelLeft->setObjectName(QString::fromUtf8("labelLeft"));
    labelLeft->setStyleSheet(QString::fromUtf8("QLabel{font-size:12px;}"));
    labelLeft->setTextFormat(Qt::RichText);
    horizontalLayoutLeft->addWidget(labelLeft);
    horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalLayoutLeft->addItem(horizontalSpacer);
    verticalLayoutLeft->addLayout(horizontalLayoutLeft);

    listViewLeft = new QListView(frameLeft);
    verticalLayoutLeft->addWidget(listViewLeft);

    frameRight = new QFrame;
    verticalLayoutRight = new QVBoxLayout(frameRight);
    labelRight = new QLabel(frameRight);
    labelRight->setStyleSheet(QString::fromUtf8("QLabel{font-size:12px;}"));
    labelRight->setTextFormat(Qt::RichText);
    verticalLayoutRight->addWidget(labelRight);
    listViewRight = new QListView(frameRight);
    verticalLayoutRight->addWidget(listViewRight);
    splitter->addWidget(frameLeft);
    splitter->addWidget(frameRight);
    splitter->setSizes(QList<int>() << 1 << 1);


    verticalLayoutLeft->setSpacing(0);
    verticalLayoutLeft->setContentsMargins(0, 0, 0, 0);
    verticalLayoutRight->setSpacing(0);
    verticalLayoutRight->setContentsMargins(0, 0, 0, 0);

    labelLeft->setMargin(5);
    labelRight->setMargin(5);

    listViewLeft->setStyleSheet(m_listViewStyle);
    listViewLeft->setResizeMode(QListView::Adjust);
    listViewLeft->setViewMode(QListView::IconMode);
    listViewLeft->setSelectionMode(QAbstractItemView::ExtendedSelection);
    listViewLeft->setUniformItemSizes(true);
    QAbstractItemModel * oldLib = listViewLeft->model();
    SnbLibrary* lib = new SnbLibrary(this);
    listViewLeft->setDragEnabled(true);
    listViewLeft->setAcceptDrops(false);
    listViewLeft->setModel(lib);
    delete oldLib;

    listViewRight->setStyleSheet(m_listViewStyle);
    listViewRight->setResizeMode(QListView::Adjust);
    listViewRight->setViewMode(QListView::IconMode);
    listViewRight->setSelectionMode(QAbstractItemView::ExtendedSelection);
    listViewRight->setUniformItemSizes(true);
    QAbstractItemModel * oldLibRight = listViewRight->model();
    listViewRight->setDragEnabled(true);
    listViewRight->setAcceptDrops(false);
    listViewRight->setModel(new SnbLibrary(this));
    delete oldLibRight;

    translateUiForLabelRight();
    translateUiForLabelLeft();
}

void MainWindow::translateUiForLabelRight()
{
    if (m_sn.isEmpty())
    {
        labelRight->setText(tr("Bambook"));
        return;
    }
    QString text = tr("Bambook: SN:%1, Firmware:%2, Total space:%3MB, Free space:%4MB");
    text = text.arg(m_sn).arg(m_fv).arg(m_totalSpaceInKb/1024).arg(m_freeSpaceInKb/1024);
    labelRight->setText(text);
}

void MainWindow::translateUiForActions()
{
    bool connected = actionConnect->isChecked();
    if (connected)
    {
        actionConnect->setText(tr("Connected"));
        actionConnect->setToolTip(tr("Bambook is connected"));
    }
    else
    {
        actionConnect->setText(tr("Unconnected"));
        actionConnect->setToolTip(tr("Bambook is unconnected"));
    }
}

void MainWindow::translateUiForLabelLeft()
{
    QString path = wstr2qstr(ZGetSnbDir());
    if (path.length() > 60)
        path = path.left(40) + "..." + path.right(20);
    labelLeft->setText(tr("My computer: %1").arg(path));
    btnRefreshLeft->setText(tr("Refresh"));
    //statusBar()->showMessage(tr("Current Version: 0.9"));
}

void MainWindow::retranslateAllUi()
{
    retranslateUi(this);
    translateUiForLabelLeft();
    translateUiForLabelRight();
    translateUiForActions();
}

void MainWindow::switchLanguage(const QString& lang)
{
    if (lang == "zh" || lang == "en")
    {
        qApp->removeTranslator(m_translater);
        m_translater->load(QString("bf_") + lang, QCoreApplication::applicationDirPath());
        qApp->installTranslator(m_translater);
        retranslateAllUi();
        m_lang = lang;
    }
}

SnbLibrary* MainWindow::snbRightLibrary()
{
    SnbLibrary* snbLib = qobject_cast<SnbLibrary*> (listViewRight->model());
    Q_ASSERT(snbLib != NULL);
    return snbLib;
}

SnbLibrary* MainWindow::snbLeftLibrary()
{
    SnbLibrary* snbLib = qobject_cast<SnbLibrary*> (listViewLeft->model());
    Q_ASSERT(snbLib != NULL);
    return snbLib;
}

void MainWindow::updateLeftLibrary()
{
    if (m_currentBookIsLeft)
        m_currentBook = L"";
    SnbLibrary* leftSnbLib = snbLeftLibrary();

    QDir dir(wstr2qstr(ZGetSnbDir()));
    dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setSorting(QDir::Name);
    dir.setNameFilters(QStringList() << "*.snb");

    leftSnbLib->beginReset();
    leftSnbLib->bookList().clear();
    QFileInfoList list = dir.entryInfoList();
    for (int i = 0; i < list.size(); ++i) 
    {
        QFileInfo fileInfo = list.at(i);
        ZBookInfo book;
        qstr2wstr(book.id, fileInfo.fileName());
        ZGetBookInfo(book.id.c_str(), book);
        leftSnbLib->bookList().push_back(book);
    }
    leftSnbLib->endReset();
    translateUiForLabelLeft();
    updateActions();
}

void MainWindow::onDeviceConnected(const wchar_t* sn, const wchar_t* fv, int totalSpaceInKb, 
    int freeSpaceInKb, const list<ZDeviceBookInfo> &booklist)
{
    actionConnect->setChecked(true);
    m_sn = wstr2qstr(sn);
    m_fv = wstr2qstr(fv);
    updateRightLibrary(totalSpaceInKb, freeSpaceInKb, booklist);
    updateActions();
}

void MainWindow::updateRightLibrary(int totalSpaceInKb, int freeSpaceInKb, 
    const list<ZDeviceBookInfo> &booklist) 
{
    m_totalSpaceInKb = totalSpaceInKb;
    m_freeSpaceInKb = freeSpaceInKb;
    m_bookCount = booklist.size();
    translateUiForLabelRight();

    SnbLibrary* lib = snbRightLibrary();

    lib->beginReset();
    lib->bookList().clear();
    list<ZDeviceBookInfo>::const_iterator it = booklist.begin();
    for (; it != booklist.end(); ++it)
    {
        ZBookInfo book;
        book.id = it->id;
        book.name = it->name;
        book.author = it->author;
        book.abstract = it->abstract;
        lib->bookList().push_back(book);
    }
    lib->endReset();
}

void MainWindow::onDeviceUpdated(int totalSpaceInKb, int freeSpaceInKb, 
    const list<ZDeviceBookInfo> &booklist)
{
    updateRightLibrary(totalSpaceInKb, freeSpaceInKb, booklist);
}

void MainWindow::onDeviceDisconnected()
{
    actionConnect->setChecked(false);
    snbRightLibrary()->beginReset();
    snbRightLibrary()->bookList().clear();
    snbRightLibrary()->endReset();
    updateActions();
    translateUiForLabelRight();
}

void MainWindow::updateActions()
{
    actionConnect->setEnabled(false);
    bool connected = actionConnect->isChecked();
    translateUiForActions();
    bool leftSelected = listViewLeft->selectionModel()->hasSelection();
    bool rightSelected = listViewRight->selectionModel()->hasSelection();
    bool selected = leftSelected || rightSelected;
    bool multiSelected = false;
    if (selected)
    {
        int count = 0;
        QItemSelection sel = (leftSelected ? listViewLeft : listViewRight)
            ->selectionModel()->selection();
        foreach (QItemSelectionRange r, sel)
        {
            count += r.width() * r.height();
        }
        multiSelected = (count > 1);
        if (!multiSelected)
        {
            SnbLibrary* lib = (SnbLibrary*)(leftSelected ? listViewLeft : listViewRight)->model();
            m_currentBook = lib->bookList().at(sel.at(0).top()).id;
            m_currentBookIsLeft = leftSelected;
        }
    }
    actionCopyToBambook->setEnabled(connected && leftSelected && !multiSelected);
    actionCopyFromBambook->setEnabled(connected && rightSelected && !multiSelected);
    actionReadTheBook->setEnabled(selected && !multiSelected);
    actionDeleteTheBook->setEnabled(selected);
}

void MainWindow::leftSelectionChanged()
{
    if (listViewLeft->selectionModel()->hasSelection())
        listViewRight->clearSelection();
    updateActions();
}

void MainWindow::rightSelectionChanged()
{
    if (listViewRight->selectionModel()->hasSelection())
        listViewLeft->clearSelection();
    updateActions();
}

void MainWindow::connectBB()
{
    if (!actionConnect->isChecked())
    {
        ZStopConnectThread();
        onDeviceDisconnected();
    }
    else
        ZStartConnectThread(onConnectedCB, onBooklistChangedCB, onDisConnectedCB);
}

void MainWindow::doubleClickedLeft(const QModelIndex & index)
{
    QString snbBook = snbLeftLibrary()->data(index, Qt::UserRole).value<QString>();
    wstring wstr;
    m_currentBook = qstr2wstr(wstr, snbBook);
    m_currentBookIsLeft = true;
    readBook();
}

void MainWindow::doubleClickedRight(const QModelIndex & index)
{
    QString snbBook = snbRightLibrary()->data(index, Qt::UserRole).value<QString>();
    wstring wstr;
    m_currentBook = qstr2wstr(wstr, snbBook);
    m_currentBookIsLeft = false;
    readBook();
}

void MainWindow::readBook()
{
    if (!m_currentBookIsLeft)
    {
        bool found = false;
        foreach (ZBookInfo book, snbLeftLibrary()->bookList())
        {
            if (book.id == m_currentBook)
            {
                found = true;
                QErrorMessage msg;
                msg.showMessage(tr("Open the same book in the library instead of the book in Bambook device"));
                msg.exec();
                break;
            }
        }
        if (!found)
        {
            QString bookName;
            foreach (ZBookInfo book, snbRightLibrary()->bookList())
            {
                if (book.id == m_currentBook)
                {
                    bookName = wstr2qstr(book.name.c_str());
                    break;
                }
            }
            QErrorMessage msg;
            msg.showMessage(tr("The book %1(%2) will be transferred to your computer before opening")
                .arg(bookName).arg(wstr2qstr(m_currentBook.c_str())));
            msg.exec();
            if (!copyFromBB(QList<wstring>() << m_currentBook))
                return;
            m_isPendingRead = true;
            return;
        }
    }
    ZReadBook(m_currentBook.c_str(), true);
}

void MainWindow::openFolder()
{
    QProcess* p = new QProcess(this);
    QString s = "explorer.exe "; // only for windows, todo: other platforms
    QString path = wstr2qstr(ZGetSnbDir());
    p->start(s + path);
}

bool MainWindow::copyToBB(const QList<ZBookInfo>& booklist)
{
    bool found = false;
    QString repeatBooks;
    QMap<wstring, wstring> repeatBookMap;
    foreach (ZBookInfo book, booklist)
    {
        foreach (ZBookInfo rightbook, snbRightLibrary()->bookList())
        {
            if (book.id == rightbook.id || book.name == rightbook.name)
            {
                if (found) // not the first one
                    repeatBooks += "\n";
                repeatBooks += QString("%1 (%2)").
                    arg(wstr2qstr(book.name.c_str())).
                    arg(wstr2qstr(book.id.c_str()));
                repeatBookMap.insert(book.id, rightbook.id);
                found = true;
                break;
            }
        }
    }

    bool bReplace = false;
    if (found)
    {
        QMessageBox msg;
        msg.setWindowTitle(tr("Replace the existing book?"));
        msg.setText(tr("Duplicated books found: %1, do you want to continue? Click Replace button to replace, click Add button to add new ones, or click cancel button to cancel this operation")
            .arg(repeatBooks));
        QPushButton *replaceButton = msg.addButton(tr("Replace the book"), QMessageBox::YesRole);
        QPushButton *addNewButton = msg.addButton(tr("Add new book"), QMessageBox::NoRole);
        QPushButton *cancelButton = msg.addButton(tr("Cancel this operation"), QMessageBox::RejectRole);
        msg.exec();
        if (msg.clickedButton() == replaceButton)
            bReplace = true;
        else if (msg.clickedButton() == addNewButton)
            bReplace = false;
        else if (msg.clickedButton() == cancelButton)
            return true;
    }

    foreach (ZBookInfo s, booklist)
    {
        wstring file = ZGetSnbDir() + s.id;
        ZTransToDevice(file.c_str(), bReplace, repeatBookMap.value(s.id).c_str(),
            transferingToDeviceCB, 0, true);
    }
    return true;
}

bool MainWindow::copyFromBB(const QList<wstring>& booklist)
{
    bool found = false;
    QString repeatBooks;
    QList<wstring> repeatBookList;
    foreach (wstring book, booklist)
    {
        foreach (ZBookInfo leftbook, snbLeftLibrary()->bookList())
        {
            if (book == leftbook.id)
            {
                if (found) // not the first one
                    repeatBooks += "\n";
                repeatBooks += QString("%1").
                    arg(wstr2qstr(book.c_str()));
                repeatBookList.push_back(book);
                found = true;
                break;
            }
        }
    }

    if (found)
    {
        QMessageBox msg;
        msg.setWindowTitle(tr("Replace the existing book?"));
        msg.setText(tr("Duplicated books found: %1, do you want to continue? Click Replace button to replace, click cancel button to cancel this operation")
            .arg(repeatBooks));
        msg.addButton(tr("Replace the book"), QMessageBox::YesRole);
        QPushButton *cancelButton = msg.addButton(tr("Cancel this operation"), QMessageBox::RejectRole);
        msg.exec();
        if (msg.clickedButton() == cancelButton)
            return true;
        foreach (wstring s, repeatBookList)
        {
            QDir dir(wstr2qstr(ZGetSnbDir()));
            //file delete
            foreach (wstring file, booklist)
            {
                dir.remove(wstr2qstr(s.c_str()));
            }
        }
    }

    foreach (wstring s, booklist)
    {
        wstring file = ZGetSnbDir() + s;
        ZTransFromDevice(s.c_str(), ZGetSnbDir(), transferingFromDeviceCB, 0);
    }
    return true;
}

bool MainWindow::deleteBook(const QList<wstring>& booklist)
{
    foreach (wstring s, booklist)
    {
        wstring file = ZGetSnbDir() + s;
        ZDeleteDeviceBook(s.c_str(), true);
    }
    return true;
}

void MainWindow::copyToBB()
{
    QItemSelection sel = listViewLeft->selectionModel()->selection();
    QList<ZBookInfo> booklist;
    foreach (QItemSelectionRange range, sel)
    {
        for (int i=range.top(); i<=range.bottom(); i++)
        {
            booklist.push_back(snbLeftLibrary()->bookList().at(i));
        }
    }
    copyToBB(booklist);
}

void MainWindow::copyFromBB()
{
    QItemSelection sel = listViewRight->selectionModel()->selection();
    QList<wstring> booklist;
    foreach (QItemSelectionRange range, sel)
    {
        for (int i=range.top(); i<=range.bottom(); i++)
        {
            booklist.push_back(snbRightLibrary()->bookList().at(i).id);
        }
    }
    copyFromBB(booklist);
}

void MainWindow::deleteBook()
{
    QMessageBox msg;
    msg.setWindowTitle(tr("Delete Book?"));
    msg.setText(tr("Do you really want to delete the books? Click Yes button to delete, click No button to cancel"));
    msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msg.setDefaultButton(QMessageBox::No);
    int ret = msg.exec();
    switch (ret) {
    case QMessageBox::Yes:
        break;
    case QMessageBox::No:
        return;
    }

    bool leftSelected = listViewLeft->selectionModel()->hasSelection();
    SnbLibrary* lib = leftSelected ? snbLeftLibrary() : snbRightLibrary();
    QItemSelection sel = (leftSelected ? listViewLeft : listViewRight)->selectionModel()->selection();
    QList<wstring> booklist;
    foreach (QItemSelectionRange range, sel)
    {
        for (int i=range.top(); i<=range.bottom(); i++)
        {
            booklist.push_back(lib->bookList().at(i).id);
        }
    }

    if (!leftSelected)
        deleteBook(booklist);
    else
    {
        QDir dir(wstr2qstr(ZGetSnbDir()));
        //file delete
        foreach (wstring file, booklist)
        {
            dir.remove(wstr2qstr(file.c_str()));
        }
        updateLeftLibrary();
    }
}

void MainWindow::options()
{
    switchLanguage(m_lang == "zh" ? "en" : "zh");
}

void MainWindow::onTransferingFromDevice(int status, int progress, const wchar_t* snbFile, void* v)
{
     onTransferingCommon(true, status, progress, snbFile, v);
}

void MainWindow::onTransferingToDevice(int status, int progress, const wchar_t* snbFile, void* v)
{
    onTransferingCommon(false, status, progress, snbFile, v);
}

void MainWindow::onTransferingCommon(bool fromDevice, int status, int progress, const wchar_t* snbFile, void*)
{
    if  (status == 1 || status == 2)
    {
        if (m_progressBar)
        {
            statusBar()->removeWidget(m_progressBar);
            delete m_progressBar;
            m_progressBar = NULL;
        }
        QMessageBox msg;
        QString str;
        if (fromDevice)
        {
            str = (status == 1) ? tr("Transferring file %1 from Bambook is finished")
                : tr("Transferring file %1 from Bambook is failed by unknown reasons");
            msg.setText(str.arg(wstr2qstr(snbFile)));
        }
        else
        {
            str = (status == 1) ? tr("Transferring file %1 to Bambook is finished")
                : tr("Transferring file %1 to Bambook is failed by unknown reasons");
            QFileInfo fi(wstr2qstr(snbFile));
            msg.setText(str.arg(fi.fileName()));
        }
        msg.setWindowTitle(tr("Transferring completed"));
        msg.exec();
        if (fromDevice && status == 1)
        {
            updateLeftLibrary();
            if (m_isPendingRead)
            {
                m_currentBook = snbFile;
                ZReadBook(m_currentBook.c_str());
                m_isPendingRead = false;
            }
        }
        return;
    }
    Q_ASSERT(status == 0);
    if (m_progressBar == NULL)
    {
        m_progressBar = new QProgressBar();
        statusBar()->addWidget(m_progressBar);
    }
    m_progressBar->setValue(progress);
}

void MainWindow::btnRefreshLeftClicked()
{
    updateLeftLibrary();
}

void MainWindow::importPDF()
{
    //QMessageBox msg;
    //msg.setIconPixmap(QPixmap(":/images/under_construction.png"));
    //msg.setWindowTitle(tr("Under construction"));
    //msg.setText(tr("This function is under contruction."));
    //msg.exec();

    PdfMainWindow window(this);
    window.exec();
    updateLeftLibrary();
}

void MainWindow::importTxt()
{
    //QMessageBox msg;
    //msg.setIconPixmap(QPixmap(":/images/under_construction.png"));
    //msg.setWindowTitle(tr("Under construction"));
    //msg.setText(tr("This function is under contruction."));
    //msg.exec();
    TxtMainWindow window(this);
    window.exec();
    updateLeftLibrary();
}