#ifndef CHAPTERS_EDITOR_H
#define CHAPTERS_EDITOR_H

#include "ui_chaptersEditor.hpp"
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QMutex>
#include "zpdf.h"

class QAbstractButton;
class ChaptersEditor : public QDialog, public Ui::ChaptersEditor
{
    Q_OBJECT
public:
    ChaptersEditor(QVector<ChapterInfo>* chapter1, QVector<ChapterInfo>* chapter2, QWidget *parent = 0);
    ~ChaptersEditor();
    bool useLeft() {return m_useLeft;}
    bool retry() {return m_retry;}
    static void InitList(QListWidget* list, QVector<ChapterInfo>* chapters);

protected Q_SLOTS:
    void clicked (QAbstractButton * button);

private:
    QVector<ChapterInfo>* m_chapter1;
    QVector<ChapterInfo>* m_chapter2;
    bool m_useLeft;
    bool m_retry;
};

#endif
