#include "stdafx.h"
#include "chaptersEditor.hxx"

void ChaptersEditor::InitList(QListWidget* listLeft, QVector<ChapterInfo>* m_chapter1)
{
    for (int i=0; i<m_chapter1->size(); i++)
    {
        QListWidgetItem *newItem = new QListWidgetItem(listLeft);
        newItem->setCheckState(Qt::Checked);
        newItem->setText(m_chapter1->at(i).title);
        QString page, line, preview;
        if (m_chapter1->at(i).pageIndex != -1)
            page = tr("Page: %1").arg(m_chapter1->at(i).pageIndex) + "\n";

        if (m_chapter1->at(i).rowIndex != -1)
            line = tr("Line %1").arg(m_chapter1->at(i).rowIndex) + "\n";
        else
            line = tr("At the beginning of the book");

        if (!m_chapter1->at(i).context.isEmpty())
        {
            preview = tr("preview:") + "\n" + m_chapter1->at(i).context;
        }
        newItem->setToolTip(page + line + preview);
    }
}


ChaptersEditor::ChaptersEditor(QVector<ChapterInfo>* chapter1, QVector<ChapterInfo>* chapter2, 
    QWidget *parent) : QDialog(parent)
{
    setupUi(this);
    m_chapter1 = chapter1;
    m_chapter2 = chapter2;

    connect(rdLeft, SIGNAL(toggled(bool)), listLeft, SLOT(setEnabled(bool)));
    connect(rdRight, SIGNAL(toggled(bool)), listRight, SLOT(setEnabled(bool)));
    connect(buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(clicked(QAbstractButton*)));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    rdLeft->setChecked(true);
    listRight->setEnabled(false);
    m_useLeft = true;
    m_retry = false;

    InitList(listLeft, m_chapter1);
    if (m_chapter2 != NULL)
        InitList(listRight, m_chapter2);

    if (m_chapter2 == NULL)
    {
        listRight->setVisible(false);
        rdRight->setVisible(false);
        rdLeft->setVisible(false);
        label->setText(tr("Please deselect wrong items."));
    }
}

ChaptersEditor::~ChaptersEditor()
{

}

void ChaptersEditor::clicked(QAbstractButton* btn)
{
    if (btn == buttonBox->button(QDialogButtonBox::Cancel))
        return;

    if (btn == buttonBox->button(QDialogButtonBox::Retry))
    {
        m_retry = true;
        QDialog::accept();
        return;
    }

    m_useLeft = listLeft->isEnabled();
    if (m_useLeft)
    {
        for(int i=listLeft->count()-1; i>=0; i--)
        {
            if (listLeft->item(i)->checkState() == Qt::Unchecked)
                m_chapter1->remove(i);
        }
    }
    else
    {
        if (m_chapter2 != NULL)
        {
            for(int i=listRight->count()-1; i>=0; i--)
            {
                if (listRight->item(i)->checkState() == Qt::Unchecked)
                    m_chapter2->remove(i);
            }
        }
    }
    QDialog::accept();
}