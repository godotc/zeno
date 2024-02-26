#include "zenosearchbar.h"
#include <zenoui/comctrl/ziconbutton.h>
#include <zenomodel/include/modelrole.h>
#include <zenoui/style/zenostyle.h>
#include "zenoapplication.h"
#include <zenomodel/include/graphsmanagment.h>
#include <zenoui/comctrl/zlabel.h>

ZenoSearchBar::ZenoSearchBar(const QModelIndex& idx, QWidget *parentWidget)
    : QWidget(parentWidget)
    , m_idx(0)
    , m_index(idx)
{
    setWindowFlag(Qt::SubWindow);
    setWindowFlag(Qt::FramelessWindowHint);

    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(36, 36, 36));
    setPalette(pal);

    m_pLineEdit = new QLineEdit;
    m_pLineEdit->setFocusPolicy(Qt::StrongFocus);
    m_pLineEdit->setObjectName("searchEdit");
    m_pLineEdit->setFixedWidth(200);
    QFont font = QApplication::font();
    m_pLineEdit->setFont(font);
    ZIconButton *pCloseBtn = new ZIconButton(
        QIcon(":/icons/closebtn.svg"),
        ZenoStyle::dpiScaledSize(QSize(20, 20)),
        QColor(61, 61, 61),
        QColor(66, 66, 66));
    ZIconButton *pSearchBackward = new ZIconButton(
        QIcon(":/icons/search_arrow_backward.svg"),
        ZenoStyle::dpiScaledSize(QSize(20, 20)),
        QColor(61, 61, 61),
        QColor(66, 66, 66));
    ZIconButton *pSearchForward = new ZIconButton(
        QIcon(":/icons/search_arrow.svg"),
        ZenoStyle::dpiScaledSize(QSize(20, 20)),
        QColor(61, 61, 61),
        QColor(66, 66, 66));
    QHBoxLayout *pEditLayout = new QHBoxLayout;
    
    m_countLabel = new ZTextLabel(this);
    m_countLabel->setTextColor(Qt::white);
    m_countLabel->setMinimumWidth(ZenoStyle::dpiScaled(30));
    m_countLabel->setText(QString("%1|%2").arg(0).arg(m_results.size()));
    pEditLayout->addWidget(m_pLineEdit);
    pEditLayout->addWidget(m_countLabel);
    pEditLayout->addWidget(pSearchBackward);
    pEditLayout->addWidget(pSearchForward);
    pEditLayout->addWidget(pCloseBtn);
    pEditLayout->setContentsMargins(
        ZenoStyle::dpiScaled(10),
        ZenoStyle::dpiScaled(6),
        ZenoStyle::dpiScaled(10),
        ZenoStyle::dpiScaled(6));

    setLayout(pEditLayout);

    connect(m_pLineEdit, SIGNAL(textChanged(const QString&)), this, SLOT(onSearchExec(const QString&)));
    connect(pSearchForward, SIGNAL(clicked()), this, SLOT(onSearchForward()));
    connect(pSearchBackward, SIGNAL(clicked()), this, SLOT(onSearchBackward()));
    connect(pCloseBtn, SIGNAL(clicked()), this, SLOT(close()));
}

SEARCH_RECORD ZenoSearchBar::_getRecord()
{
    QModelIndex idx = m_results[m_idx];
    const QString &nodeid = idx.data(ROLE_OBJID).toString();
    const QPointF &pos = idx.data(ROLE_OBJPOS).toPointF();
    return {nodeid, pos};
}

void ZenoSearchBar::onSearchExec(const QString& content)
{
    if (content.isEmpty()) {
        return;
    }
    IGraphsModel* pGraphsModel = zenoApp->graphsManagment()->currentModel();

    m_results = pGraphsModel->searchInSubgraph(content, m_index);
    if (!m_results.isEmpty())
    {
        m_idx = 0;
        m_countLabel->setText(QString("%1|%2").arg(m_idx + 1).arg(m_results.size()));
        SEARCH_RECORD rec = _getRecord();
        emit searchReached(rec);
    }
}

void ZenoSearchBar::onSearchForward()
{
    if (++m_idx == m_results.size())
        m_idx = 0;
    if (!m_results.isEmpty() && m_idx < m_results.size())
    {
        m_countLabel->setText(QString("%1|%2").arg(m_idx + 1).arg(m_results.size()));
        SEARCH_RECORD rec = _getRecord();
        emit searchReached(rec);
    }
}

void ZenoSearchBar::onSearchBackward()
{
    if (--m_idx < 0)
        m_idx = m_results.size() - 1;

    if (!m_results.isEmpty() && m_idx < m_results.size())
    {
        m_countLabel->setText(QString("%1|%2").arg(m_idx + 1).arg(m_results.size()));
        SEARCH_RECORD rec = _getRecord();
        emit searchReached(rec);
    }
}

void ZenoSearchBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (QWidget* par = parentWidget())
    {
		QSize sz = event->size();
		int w = par->width();
		int h = par->height();
		setGeometry(w - sz.width(), 0, sz.width(), sz.height());
    }
}

void ZenoSearchBar::keyPressEvent(QKeyEvent* event)
{
    QWidget::keyPressEvent(event);
    if (event->key() == Qt::Key_Escape)
    {
        hide();
    }
    else if (event->key() == Qt::Key_F3)
    {
        onSearchForward();
    }
    else if ((event->modifiers() & Qt::ShiftModifier) && event->key() == Qt::Key_F3)
    {
        onSearchBackward();
    }
}

void ZenoSearchBar::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    m_pLineEdit->setFocus();
}

void ZenoSearchBar::activate()
{
    show();
    m_pLineEdit->setFocus();
}
