#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ui_mainWindow.h"
#include "zconnect.h"
#include "zdisplay.h"

class SnbLibrary : public QAbstractListModel
{
    Q_OBJECT

public:
    SnbLibrary(QObject *parent = 0);
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const;
    virtual QMimeData *mimeData(const QModelIndexList &indexes) const;
    virtual QStringList mimeTypes() const;

    void beginReset();
    QList<ZBookInfo>& bookList() {return m_bookList;}
    void endReset();
private:
    QList<ZBookInfo> m_bookList;
};

class QSplitter;
class QListView;
class QFrame;
class QLabel;
class QSpacerItem;
class QHBoxLayout;
class QPushButton;

class QItemSelectionModel;
class QTranslator;
class QProgressBar;

class MainWindow : public QMainWindow, public Ui::MainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void onDeviceConnected(const wchar_t* sn, const wchar_t* fv, int totalSpaceInKb, 
        int freeSpaceInKb, const list<ZDeviceBookInfo> &booklist);
    void onDeviceUpdated(int totalSpaceInKb, int freeSpaceInKb, 
        const list<ZDeviceBookInfo> &booklist);
    void onDeviceDisconnected();
    void onTransferingFromDevice(int status, int progress, const wchar_t* bookId, void* cbParam);
    void onTransferingToDevice(int status, int progress, const wchar_t* snbFile, void* cbParam);
    
    void updateLeftLibrary();

public Q_SLOTS:
    void connectBB();
    void copyToBB();
    void copyFromBB();
    void readBook();
    void deleteBook();
    void options();
    void openFolder();
    void importPDF();
    void importTxt();
    void leftSelectionChanged();
    void rightSelectionChanged();
    void doubleClickedLeft(const QModelIndex & index);
    void doubleClickedRight(const QModelIndex & index);
    void btnRefreshLeftClicked();

protected:
    virtual void closeEvent(QCloseEvent *);

private:
    void setupUi2();
    void updateActions();
    void updateRightLibrary(int totalSpaceInKb, int freeSpaceInKb, const list<ZDeviceBookInfo> &booklist);
    SnbLibrary* snbRightLibrary();
    SnbLibrary* snbLeftLibrary();
    void translateUiForLabelRight();
    void translateUiForLabelLeft();
    void translateUiForActions();
    void retranslateAllUi();
    void switchLanguage(const QString& lang);

    bool copyToBB(const QList<ZBookInfo>& booklist);
    bool copyFromBB(const QList<wstring>& booklist);
    bool deleteBook(const QList<wstring>& booklist);
    void onTransferingCommon(bool fromDevice, int status, int progress, const wchar_t* snbFile, void*);

    QSplitter *splitter;
    QFrame *frameLeft;
    QVBoxLayout *verticalLayoutLeft;
    QLabel *labelLeft;
    QListView *listViewLeft;
    QFrame *frameRight;
    QVBoxLayout *verticalLayoutRight;
    QLabel *labelRight;
    QListView *listViewRight;
    QHBoxLayout *horizontalLayoutLeft;
    QPushButton *btnRefreshLeft;
    QSpacerItem *horizontalSpacer;

    QTranslator *m_translater;
    QString m_listViewStyle;
    QString m_sn;
    QString m_fv;
    int m_totalSpaceInKb;
    int m_freeSpaceInKb;
    int m_bookCount;
    QString m_lang;
    wstring m_currentBook;
    bool m_currentBookIsLeft;
    QString m_statusString;
    QProgressBar* m_progressBar;
    bool m_isPendingRead;
};

#endif
