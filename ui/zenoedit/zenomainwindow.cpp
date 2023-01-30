#include "zenomainwindow.h"
#include "dock/zenodockwidget.h"
#include <zenomodel/include/graphsmanagment.h>
#include "launch/corelaunch.h"
#include "launch/serialize.h"
#include "nodesview/zenographseditor.h"
#include "dock/ztabdockwidget.h"
#include "dock/docktabcontent.h"
#include "panel/zenodatapanel.h"
#include "panel/zenoproppanel.h"
#include "panel/zenospreadsheet.h"
#include "panel/zlogpanel.h"
#include "timeline/ztimeline.h"
#include "tmpwidgets/ztoolbar.h"
#include "viewport/viewportwidget.h"
#include "viewport/zenovis.h"
#include "zenoapplication.h"
#include <zeno/utils/log.h>
#include <zeno/utils/envconfig.h>
#include <zenoio/reader/zsgreader.h>
#include <zenoio/writer/zsgwriter.h>
#include <zeno/core/Session.h>
#include <zenovis/DrawOptions.h>
#include <zenomodel/include/modeldata.h>
#include <zenoui/style/zenostyle.h>
#include <zenomodel/include/uihelper.h>
#include "util/log.h"
#include "dialog/zfeedbackdlg.h"
#include "startup/zstartup.h"
#include "settings/zsettings.h"
#include "panel/zenolights.h"
#include "nodesys/zenosubgraphscene.h"
#include "viewport/recordvideomgr.h"
#include "ui_zenomainwindow.h"
#include <QJsonDocument>

const QString g_latest_layout = "LatestLayout";

ZenoMainWindow::ZenoMainWindow(QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags)
    , m_bInDlgEventloop(false)
    , m_pTimeline(nullptr)
    , m_layoutRoot(nullptr)
    , m_nResizeTimes(0)
{
    init();
    setContextMenuPolicy(Qt::NoContextMenu);
    setWindowTitle("Zeno Editor (" + QString::fromStdString(getZenoVersion()) + ")");
//#ifdef __linux__
    if (char *p = zeno::envconfig::get("OPEN")) {
        zeno::log_info("ZENO_OPEN: {}", p);
        openFile(p);
    }
//#endif
}

ZenoMainWindow::~ZenoMainWindow()
{
}

void ZenoMainWindow::init()
{
    m_ui = new Ui::MainWindow;
    m_ui->setupUi(this);

    initMenu();
    initLive();
    initDocks();

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(11, 11, 11));
    setAutoFillBackground(true);
    setPalette(pal);

    m_ui->statusbar->showMessage(tr("Status Bar"));
}

void ZenoMainWindow::initLive() {

}

void ZenoMainWindow::initMenu()
{
    //to merge:
/*
        QAction *pAction = new QAction(tr("New"), pFile);
        pAction->setCheckable(false);
        pAction->setShortcut(QKeySequence(("Ctrl+N")));
        pAction->setShortcutContext(Qt::ApplicationShortcut);
        //QMenu *pNewMenu = new QMenu;
        //QAction *pNewGraph = pNewMenu->addAction("New Scene");
        connect(pAction, SIGNAL(triggered()), this, SLOT(onNewFile()));
 */
    setActionProperty();

    QSettings settings(zsCompanyName, zsEditor);
    QVariant use_chinese = settings.value("use_chinese");
    m_ui->actionEnglish_Chinese->setChecked(use_chinese.isNull() || use_chinese.toBool());

    auto actions = findChildren<QAction*>(QString(), Qt::FindDirectChildrenOnly);
    for (QAction* action : actions)
    {
        connect(action, SIGNAL(triggered(bool)), this, SLOT(onMenuActionTriggered(bool)));  
        setActionIcon(action);
    }

    QActionGroup *actionGroup = new QActionGroup(this);
    actionGroup->addAction(m_ui->actionShading);
    actionGroup->addAction(m_ui->actionSolid);
    actionGroup->addAction(m_ui->actionOptix);

    m_ui->menubar->setProperty("cssClass", "mainWin");

    //check user saved layout.
    loadSavedLayout();
}

void ZenoMainWindow::onMenuActionTriggered(bool bTriggered)
{
    QAction* pAction = qobject_cast<QAction*>(sender());
    int actionType = pAction->property("ActionType").toInt();
    if (actionType == ACTION_SHADONG || actionType == ACTION_SOLID || actionType == ACTION_OPTIX) 
    {
        setActionIcon(m_ui->actionShading);
        setActionIcon(m_ui->actionSolid);
        setActionIcon(m_ui->actionOptix);
    } 
    else 
    {
        setActionIcon(pAction);
    }
    switch (actionType)
    {
    case ACTION_NEW: {
        onNewFile();
        break;
    }
    case ACTION_OPEN: {
        openFileDialog();
        break;
    }
    case ACTION_SAVE: {
        save();
        break;
    }
    case ACTION_SAVE_AS: {
        saveAs();
        break;
    }
    case ACTION_IMPORT: {
        importGraph();
        break;
    }
    case ACTION_EXPORT_GRAPH: {
        exportGraph();
        break;
    }
    case ACTION_CLOSE: {
        saveQuit();
        break;
    }
    case ACTION_SAVE_LAYOUT: {
        saveDockLayout();
        break;
    }
    case ACTION_LANGUAGE: {
        onLangChanged(bTriggered);
        break;
    }
    case ACTION_SCREEN_SHOOT: {
        screenShoot();
        break;
    }
    default: {
        dispatchCommand(pAction, bTriggered);
        break;
    }
    }
}

void ZenoMainWindow::dispatchCommand(QAction* pAction, bool bTriggered)
{
    if (!pAction)
        return;

    //dispatch to every panel.
    auto docks = findChildren<ZTabDockWidget*>(QString(), Qt::FindDirectChildrenOnly);
    DisplayWidget* pViewport = nullptr;
    ZenoGraphsEditor* pEditor = nullptr;
    for (ZTabDockWidget* pDock : docks)
    {
        if (!pViewport)
            pViewport = pDock->getUniqueViewport();
        if (!pEditor)
            pEditor = pDock->getAnyEditor();
    }
    if (pEditor)
    {
        pEditor->onCommandDispatched(pAction, bTriggered);
    }
    if (pViewport)
    {
        int actionType = pAction->property("ActionType").toInt();
        pViewport->onCommandDispatched(actionType, bTriggered);
    }
}

void ZenoMainWindow::loadSavedLayout()
{
	//default layout
    QJsonObject obj = readDefaultLayout();
    QStringList lst = obj.keys();
    initCustomLayoutAction(lst, true);
	//custom layout
    QSettings settings(QSettings::UserScope, zsCompanyName, zsEditor);
    settings.beginGroup("layout");
    lst = settings.childGroups();
    if (!lst.isEmpty()) {
        initCustomLayoutAction(lst, false);
    }
}

void ZenoMainWindow::saveDockLayout()
{
    bool bOk = false;
    QString name = QInputDialog::getText(this, tr("Save Layout"), tr("layout name:"),
        QLineEdit::Normal, "layout_1", &bOk);
    if (bOk)
    {
        QSettings settings(QSettings::UserScope, zsCompanyName, zsEditor);
        settings.beginGroup("layout");
        if (settings.childGroups().indexOf(name) != -1)
        {
            QMessageBox msg(QMessageBox::Question, "", tr("alreday has same layout, override?"),
                QMessageBox::Ok | QMessageBox::Cancel);
            int ret = msg.exec();
            if (ret == QMessageBox::Cancel)
            {
                settings.endGroup();
                return;
            }
        }

        QString layoutInfo = exportLayout(m_layoutRoot, size());
        settings.beginGroup(name);
        settings.setValue("content", layoutInfo);
        settings.endGroup();
        settings.endGroup();
        m_ui->menuCustom_Layout->clear();
        loadSavedLayout();
    }
}

void ZenoMainWindow::saveLayout2()
{
    auto docks = findChildren<ZTabDockWidget *>(QString(), Qt::FindDirectChildrenOnly);
    QLayout* pLayout = this->layout();
    //QMainWindowLayout* pWinLayout = qobject_cast<QMainWindowLayout*>(pLayout);
    DlgInEventLoopScope;
    QString path = QFileDialog::getSaveFileName(this, "Path to Save", "", "JSON file(*.json);;");
    writeLayout(m_layoutRoot, size(), path);
}

void ZenoMainWindow::onLangChanged(bool bChecked)
{
    QSettings settings(zsCompanyName, zsEditor);
    settings.setValue("use_chinese", bChecked);
    QMessageBox msg(QMessageBox::Information, tr("Language"),
        tr("Please restart Zeno to apply changes."),
        QMessageBox::Ok, this);
    msg.exec();
}

void ZenoMainWindow::resetDocks(PtrLayoutNode root)
{
    if (root == nullptr)
        return;

    ZTabDockWidget* cake = nullptr;

    m_layoutRoot.reset();
    auto docks = findChildren<ZTabDockWidget *>(QString(), Qt::FindDirectChildrenOnly);
    for (ZTabDockWidget *pDock : docks) {
        if (pDock->getUniqueViewport()) {
            //because of the unsteadiness of create/delete viewport widget,
            //we keep the viewport dock, and divide docks based on it.
            cake = pDock;
        }
        else {
            pDock->close();
            delete pDock;
        }
    }

    m_layoutRoot = root;
    if (!cake)
    {
        cake = new ZTabDockWidget(this);
        addDockWidget(Qt::TopDockWidgetArea, cake);
    }
    initDocksWidget(cake, m_layoutRoot);
    m_nResizeTimes = 2;
}

void ZenoMainWindow::_resizeDocks(PtrLayoutNode root)
{
    if (!root)
        return;

    if (root->type == NT_ELEM)
    {
        if (root->geom.width() > 0) {
            int W = size().width() * root->geom.width();
            resizeDocks({root->pWidget}, {W}, Qt::Horizontal);
        }
        if (root->geom.height() > 0){
            int H = size().height() * root->geom.height();
            resizeDocks({root->pWidget}, {H}, Qt::Vertical);
        }
    }
    else
    {
        _resizeDocks(root->pLeft);
        _resizeDocks(root->pRight);
    }
}

void ZenoMainWindow::initDocksWidget(ZTabDockWidget* pLeft, PtrLayoutNode root)
{
    if (!root)
        return;

    if (root->type == NT_HOR || root->type == NT_VERT)
    {
        ZTabDockWidget* pRight = new ZTabDockWidget(this);
        Qt::Orientation ori = root->type == NT_HOR ? Qt::Horizontal : Qt::Vertical;
        splitDockWidget(pLeft, pRight, ori);
        initDocksWidget(pLeft, root->pLeft);
        initDocksWidget(pRight, root->pRight);
    }
    else if (root->type == NT_ELEM)
    {
        root->pWidget = pLeft;
        for (QString tab : root->tabs)
        {
            PANEL_TYPE type = title2Type(tab);
            if (type != PANEL_EMPTY)
            {
                pLeft->onAddTab(type);
            }
        }
    }
}

PANEL_TYPE ZenoMainWindow::title2Type(const QString &title) 
{
    PANEL_TYPE type = PANEL_EMPTY;
    if (title == "Parameter") {
        type = PANEL_NODE_PARAMS;
    } else if (title == "View") {
        type = PANEL_VIEW;
    } else if (title == "Editor") {
        type = PANEL_EDITOR;
    } else if (title == "Data") {
        type = PANEL_NODE_DATA;
    } else if (title == "Logger") {
        type = PANEL_LOG;
    }
    return type;
}

void ZenoMainWindow::initCustomLayoutAction(const QStringList &list, bool isDefault) 
{
    if (!isDefault) {
        m_ui->menuCustom_Layout->addSeparator();
	}
    for (QString name : list) {
		if (name == g_latest_layout)
		{
			continue;
		}
        QAction *pCustomLayout_ = new QAction(name);
        connect(pCustomLayout_, &QAction::triggered, this, [=]() { 
			loadDockLayout(name, isDefault);
		});
        m_ui->menuCustom_Layout->addAction(pCustomLayout_);
    }
}

void ZenoMainWindow::loadDockLayout(QString name, bool isDefault) 
{
    QString content;
    if (isDefault) 
	{
        QJsonObject obj = readDefaultLayout();
        for (QJsonObject::const_iterator it = obj.constBegin(); it != obj.constEnd(); it++) 
		{
            if (it.key() == name) 
			{
                QJsonObject layout = it.value().toObject();
                QJsonDocument doc(layout);
                content = doc.toJson();
                break;
            }
        }
    } 
	else 
	{
        QSettings settings(QSettings::UserScope, zsCompanyName, zsEditor);
        settings.beginGroup("layout");
        settings.beginGroup(name);
        if (settings.allKeys().indexOf("content") != -1) 
		{
            content = settings.value("content").toString();
            settings.endGroup();
            settings.endGroup();
        } 
		else
		{
            loadDockLayout("Default", true);
            return;
        }
    }
    if (!content.isEmpty()) 
	{
        PtrLayoutNode root = readLayout(content);
        resetDocks(root);
    } 
	else 
	{
        QMessageBox msg(QMessageBox::Warning, "", tr("layout format is invalid."));
        msg.exec();
    }
}

QJsonObject ZenoMainWindow::readDefaultLayout() 
{
    QString filename = ":/templates/DefaultLayout.txt";
    QFile file(filename);
    bool ret = file.open(QIODevice::ReadOnly | QIODevice::Text);
    if (!ret) {
        return QJsonObject();
    }
    QByteArray byteArray = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(byteArray);
    if (doc.isObject()) {
        return doc.object();
    }
    return QJsonObject();
}

void ZenoMainWindow::initDocks()
{
    /*m_layoutRoot = std::make_shared<LayerOutNode>();
    m_layoutRoot->type = NT_ELEM;

    ZTabDockWidget* viewDock = new ZTabDockWidget(this);
    viewDock->setCurrentWidget(PANEL_VIEW);
    viewDock->setObjectName("viewDock");

    ZTabDockWidget *logDock = new ZTabDockWidget(this);
    logDock->setCurrentWidget(PANEL_LOG);
    logDock->setObjectName("logDock");

    ZTabDockWidget *paramDock = new ZTabDockWidget(this);
    paramDock->setCurrentWidget(PANEL_NODE_PARAMS);
    paramDock->setObjectName("paramDock");

    ZTabDockWidget* editorDock = new ZTabDockWidget(this);
    editorDock->setCurrentWidget(PANEL_EDITOR);
    editorDock->setObjectName("editorDock");

    addDockWidget(Qt::TopDockWidgetArea, viewDock);
    m_layoutRoot->type = NT_ELEM;
    m_layoutRoot->pWidget = viewDock;

    SplitDockWidget(viewDock, editorDock, Qt::Vertical);
    SplitDockWidget(viewDock, logDock, Qt::Horizontal);
    SplitDockWidget(editorDock, paramDock, Qt::Horizontal);

    //paramDock->hide();
    logDock->hide();*/

	loadDockLayout(g_latest_layout, false);
    initTimelineDock();
}

void ZenoMainWindow::initTimelineDock()
{
    m_pTimeline = new ZTimeline;
    setCentralWidget(m_pTimeline);

    connect(m_pTimeline, &ZTimeline::playForward, this, [=](bool bPlaying) {
        auto docks = findChildren<ZTabDockWidget*>(QString(), Qt::FindDirectChildrenOnly);
        for (ZTabDockWidget* pDock : docks)
            pDock->onPlayClicked(bPlaying);
    });

    connect(m_pTimeline, &ZTimeline::sliderValueChanged, this, [=](int frame) {
        auto docks = findChildren<ZTabDockWidget*>(QString(), Qt::FindDirectChildrenOnly);
        for (ZTabDockWidget* pDock : docks)
            pDock->onSliderValueChanged(frame);
    });

    connect(m_pTimeline, &ZTimeline::run, this, [=]() {
        auto docks = findChildren<ZTabDockWidget *>(QString(), Qt::FindDirectChildrenOnly);
        for (ZTabDockWidget *pDock : docks)
            pDock->onRun();
    });

    connect(m_pTimeline, &ZTimeline::kill, this, [=]() {
        auto docks = findChildren<ZTabDockWidget *>(QString(), Qt::FindDirectChildrenOnly);
        for (ZTabDockWidget *pDock : docks)
            pDock->onKill();
    });

    connect(m_pTimeline, &ZTimeline::alwaysChecked, this, [=]() {
        auto docks = findChildren<ZTabDockWidget *>(QString(), Qt::FindDirectChildrenOnly);
        for (ZTabDockWidget *pDock : docks)
            pDock->onRun();
    });

    auto graphs = zenoApp->graphsManagment();
    connect(graphs, &GraphsManagment::modelDataChanged, this, [=]() {
        if (m_pTimeline->isAlways()) {
            auto docks = findChildren<ZTabDockWidget *>(QString(), Qt::FindDirectChildrenOnly);
            for (ZTabDockWidget *pDock : docks)
                pDock->onRun();
        }
    });
}

ZTimeline* ZenoMainWindow::timeline() const
{
    return m_pTimeline;
}

void ZenoMainWindow::onMaximumTriggered()
{
    ZTabDockWidget* pDockWidget = qobject_cast<ZTabDockWidget*>(sender());
    auto docks = findChildren<ZTabDockWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (ZTabDockWidget* pDock : docks)
    {
        if (pDock != pDockWidget)
        {
            pDock->close();
        }
    }
}


void ZenoMainWindow::directlyRunRecord(const ZENO_RECORD_RUN_INITPARAM& param)
{
#if 0
    ZASSERT_EXIT(m_viewDock);
    DisplayWidget* viewWidget = qobject_cast<DisplayWidget *>(m_viewDock->widget());
    ZASSERT_EXIT(viewWidget);
    ViewportWidget* pViewport = viewWidget->getViewportWidget();
    ZASSERT_EXIT(pViewport);

    //hide other component
    if (m_editor) m_editor->hide();
    if (m_logger) m_logger->hide();
    if (m_parameter) m_parameter->hide();

    VideoRecInfo recInfo;
    recInfo.bitrate = param.iBitrate;
    recInfo.fps = param.iFps;
    recInfo.frameRange = {param.iSFrame, param.iSFrame + param.iFrame - 1};
    recInfo.numMSAA = 0;
    recInfo.numOptix = 1;
    recInfo.numSamples = param.iSample;
    recInfo.audioPath = param.audioPath;
    recInfo.record_path = param.sPath;
    recInfo.bRecordRun = true;
    recInfo.videoname = "output.mp4";
    recInfo.exitWhenRecordFinish = param.exitWhenRecordFinish;

    if (!param.sPixel.isEmpty())
    {
        QStringList tmpsPix = param.sPixel.split("x");
        int pixw = tmpsPix.at(0).toInt();
        int pixh = tmpsPix.at(1).toInt();
        recInfo.res = {(float)pixw, (float)pixh};

        pViewport->setFixedSize(pixw, pixh);
        pViewport->setCameraRes(QVector2D(pixw, pixh));
        pViewport->updatePerspective();
    } else {
        recInfo.res = {(float)1000, (float)680};
        pViewport->setMinimumSize(1000, 680);
    }

    auto sess = Zenovis::GetInstance().getSession();
    if (sess) {
        auto scene = sess->get_scene();
        if (scene) {
            scene->drawOptions->num_samples = param.bRecord ? param.iSample : 16;
        }
    }

    bool ret = openFile(param.sZsgPath);
    ZASSERT_EXIT(ret);
    viewWidget->runAndRecord(recInfo);
#endif
}

void ZenoMainWindow::updateViewport(const QString& action)
{
    auto docks2 = findChildren<ZTabDockWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto dock : docks2)
    {
        dock->onUpdateViewport(action);
    }
}

DisplayWidget* ZenoMainWindow::getDisplayWidget()
{
    //DisplayWidget* view = qobject_cast<DisplayWidget*>(m_viewDock->widget());
    //if (view)
    //    return view;
    return nullptr;
}

void ZenoMainWindow::onRunFinished()
{
    auto docks2 = findChildren<ZTabDockWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto dock : docks2)
    {
        dock->onRunFinished();
    }
}

void ZenoMainWindow::onCloseDock()
{
    ZTabDockWidget *pDockWidget = qobject_cast<ZTabDockWidget *>(sender());
    ZASSERT_EXIT(pDockWidget);
    pDockWidget->close();

    PtrLayoutNode spParent = findParent(m_layoutRoot, pDockWidget);
    if (spParent)
    {
        if (spParent->pLeft->pWidget == pDockWidget)
        {
            PtrLayoutNode right = spParent->pRight;
            spParent->pWidget = right->pWidget;
            spParent->pLeft = right->pLeft;
            spParent->pRight = right->pRight;
            spParent->type = right->type;
        }
        else if (spParent->pRight->pWidget == pDockWidget)
        {
            PtrLayoutNode left = spParent->pLeft;
            spParent->pWidget = left->pWidget;
            spParent->pLeft = left->pLeft;
            spParent->pRight = left->pRight;
            spParent->type = left->type;
        }
    }
    else
    {
        m_layoutRoot = nullptr;
    }
}

void ZenoMainWindow::SplitDockWidget(ZTabDockWidget* after, ZTabDockWidget* dockwidget, Qt::Orientation orientation)
{
    splitDockWidget(after, dockwidget, orientation);

    PtrLayoutNode spRoot = findNode(m_layoutRoot, after);
    ZASSERT_EXIT(spRoot);

    spRoot->type = (orientation == Qt::Vertical ? NT_VERT : NT_HOR);
    spRoot->pWidget = nullptr;

    spRoot->pLeft = std::make_shared<LayerOutNode>();
    spRoot->pLeft->pWidget = after;
    spRoot->pLeft->type = NT_ELEM;

    spRoot->pRight = std::make_shared<LayerOutNode>();
    spRoot->pRight->pWidget = dockwidget;
    spRoot->pRight->type = NT_ELEM;
}

void ZenoMainWindow::onSplitDock(bool bHorzontal)
{
    ZTabDockWidget* pDockWidget = qobject_cast<ZTabDockWidget*>(sender());
    ZTabDockWidget* pDock = new ZTabDockWidget(this);

    //QLayout* pLayout = this->layout();
    //QMainWindowLayout* pWinLayout = qobject_cast<QMainWindowLayout*>(pLayout);

    pDock->setObjectName("editorDock233");
    pDock->setCurrentWidget(PANEL_EDITOR);
    //pDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable);
    SplitDockWidget(pDockWidget, pDock, bHorzontal ? Qt::Horizontal : Qt::Vertical);
}

void ZenoMainWindow::openFileDialog()
{
    QString filePath = getOpenFileByDialog();
    if (filePath.isEmpty())
        return;

    //todo: path validation
    if (saveQuit()) 
    {
        openFile(filePath);
    }
}

void ZenoMainWindow::onNewFile() {
    if (saveQuit()) 
    {
        zenoApp->graphsManagment()->newFile();
    }
}

void ZenoMainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
}

void ZenoMainWindow::closeEvent(QCloseEvent *event)
{
    bool isClose = this->saveQuit();
    // todo: event->ignore() when saveQuit returns false?
    if (isClose) 
    {
		//save latest layout
        QSettings settings(QSettings::UserScope, zsCompanyName, zsEditor);
        settings.beginGroup("layout");
        QString layoutInfo = exportLayout(m_layoutRoot, size());
        settings.beginGroup(g_latest_layout);
        settings.setValue("content", layoutInfo);
        settings.endGroup();
        settings.endGroup();

        QMainWindow::closeEvent(event);
    } 
    else 
    {
        event->ignore();
    }
}

bool ZenoMainWindow::event(QEvent* event)
{
    if (QEvent::LayoutRequest == event->type())
    {
        //resizing have to be done after fitting layout, which follows by LayoutRequest.
        //it seems that after `m_nResizeTimes` times, the resize action can be valid...
        if (m_nResizeTimes > 0 && m_layoutRoot)
        {
            --m_nResizeTimes;
            if (m_nResizeTimes == 0)
            {
                _resizeDocks(m_layoutRoot);
                return true;
            }
        }
    }
    return QMainWindow::event(event);
}

void ZenoMainWindow::importGraph() {
    QString filePath = getOpenFileByDialog();
    if (filePath.isEmpty())
        return;

    //todo: path validation
    auto pGraphs = zenoApp->graphsManagment();
    pGraphs->importGraph(filePath);
}

static bool saveContent(const QString &strContent, QString filePath) {
    QFile f(filePath);
    zeno::log_debug("saving {} chars to file [{}]", strContent.size(), filePath.toStdString());
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << Q_FUNC_INFO << "Failed to open" << filePath << f.errorString();
        zeno::log_error("Failed to open file for write: {} ({})", filePath.toStdString(),
                        f.errorString().toStdString());
        return false;
    }
    f.write(strContent.toUtf8());
    f.close();
    zeno::log_debug("saved successfully");
    return true;
}

void ZenoMainWindow::exportGraph() {
    DlgInEventLoopScope;
    QString path = QFileDialog::getSaveFileName(this, "Path to Export", "",
                                                "C++ Source File(*.cpp);; JSON file(*.json);; All Files(*);;");
    if (path.isEmpty()) {
        return;
    }

    //auto pGraphs = zenoApp->graphsManagment();
    //pGraphs->importGraph(path);

    QString content;
    {
        IGraphsModel *pModel = zenoApp->graphsManagment()->currentModel();
        if (path.endsWith(".cpp")) {
            content = serializeSceneCpp(pModel);
        } else {
            rapidjson::StringBuffer s;
            RAPIDJSON_WRITER writer(s);
            writer.StartArray();
            serializeScene(pModel, writer);
            writer.EndArray();
            content = QString(s.GetString());
        }
    }
    saveContent(content, path);
}

bool ZenoMainWindow::openFile(QString filePath)
{
    auto pGraphs = zenoApp->graphsManagment();
    IGraphsModel* pModel = pGraphs->openZsgFile(filePath);
    if (!pModel)
        return false;

    resetTimeline(pGraphs->timeInfo());
    recordRecentFile(filePath);
    return true;
}

void ZenoMainWindow::recordRecentFile(const QString& filePath)
{
    QSettings settings(QSettings::UserScope, zsCompanyName, zsEditor);
    settings.beginGroup("Recent File List");

    QStringList keys = settings.childKeys();
    QStringList paths;
    for (QString key : keys) {
        QString path = settings.value(key).toString();
        if (path == filePath)
        {
            //remove the old record.
            settings.remove(key);
            continue;
        }
        paths.append(path);
    }

    if (paths.indexOf(filePath) != -1) {
        return;
    }

    int idx = -1;
    if (keys.isEmpty()) {
        idx = 0;
    } else {
        for (QString key : keys) {
            static QRegExp rx("File (\\d+)");
            if (rx.indexIn(key) != -1) {
                QStringList caps = rx.capturedTexts();
                if (caps.length() == 2 && idx < caps[1].toInt())
                    idx = caps[1].toInt();
            }
        }
    }

    settings.setValue(QString("File %1").arg(idx + 1), filePath);
}

void ZenoMainWindow::onToggleDockWidget(DOCK_TYPE type, bool bShow)
{
    auto docks = findChildren<ZenoDockWidget *>(QString(), Qt::FindDirectChildrenOnly);
    for (ZenoDockWidget *dock : docks)
    {
        DOCK_TYPE _type = dock->type();
        if (_type == type)
            dock->setVisible(bShow);
    }
}

QString ZenoMainWindow::uniqueDockObjName(DOCK_TYPE type)
{
    switch (type)
    {
    case DOCK_EDITOR: return UiHelper::generateUuid("dock_editor_");
    case DOCK_LOG: return UiHelper::generateUuid("dock_log_");
    case DOCK_NODE_DATA: return UiHelper::generateUuid("dock_data_");
    case DOCK_VIEW: return UiHelper::generateUuid("dock_view_");
    case DOCK_NODE_PARAMS: return UiHelper::generateUuid("dock_parameter_");
    case DOCK_LIGHTS: return UiHelper::generateUuid("dock_lights_");
    default:
        return UiHelper::generateUuid("dock_empty_");
    }
}

void ZenoMainWindow::setActionProperty() 
{
    m_ui->action_New->setProperty("ActionType", ACTION_NEW);
    m_ui->action_Open->setProperty("ActionType", ACTION_OPEN);
    m_ui->action_Save->setProperty("ActionType", ACTION_SAVE);
    m_ui->action_Save_As->setProperty("ActionType", ACTION_SAVE_AS);
    m_ui->action_Import->setProperty("ActionType", ACTION_IMPORT);
    m_ui->actionExportGraph->setProperty("ActionType", ACTION_EXPORT_GRAPH);
    m_ui->actionScreen_Shoot->setProperty("ActionType", ACTION_SCREEN_SHOOT);
    m_ui->actionRecord_Video->setProperty("ActionType", ACTION_RECORD_VIDEO);
    m_ui->action_Close->setProperty("ActionType", ACTION_CLOSE);
    m_ui->actionUndo->setProperty("ActionType", ACTION_UNDO);
    m_ui->actionRedo->setProperty("ActionType", ACTION_REDO);
    m_ui->action_Copy->setProperty("ActionType", ACTION_COPY);
    m_ui->action_Paste->setProperty("ActionType", ACTION_PASTE);
    m_ui->action_Cut->setProperty("ActionType", ACTION_CUT);
    m_ui->actionCollaspe->setProperty("ActionType", ACTION_COLLASPE);
    m_ui->actionExpand->setProperty("ActionType", ACTION_EXPAND);
    m_ui->actionEasy_Graph->setProperty("ActionType", ACTION_EASY_GRAPH);
    m_ui->actionOpen_View->setProperty("ActionType", ACTION_OPEN_VIEW);
    m_ui->actionClear_View->setProperty("ActionType", ACTION_CLEAR_VIEW);
    m_ui->actionSmooth_Shading->setProperty("ActionType", ACTION_SMOOTH_SHADING);
    m_ui->actionNormal_Check->setProperty("ActionType", ACTION_NORMAL_CHECK);
    m_ui->actionWireFrame->setProperty("ActionType", ACTION_WIRE_FRAME);
    m_ui->actionShow_Grid->setProperty("ActionType", ACTION_SHOW_GRID);
    m_ui->actionBackground_Color->setProperty("ActionType", ACTION_BACKGROUND_COLOR);
    m_ui->actionSolid->setProperty("ActionType", ACTION_SOLID);
    m_ui->actionShading->setProperty("ActionType", ACTION_SHADONG);
    m_ui->actionOptix->setProperty("ActionType", ACTION_OPTIX);
    m_ui->actionBlackWhite->setProperty("ActionType", ACTION_BLACK_WHITE);
    m_ui->actionCreek->setProperty("ActionType", ACTION_GREEK);
    m_ui->actionDay_Light->setProperty("ActionType", ACTION_DAY_LIGHT);
    m_ui->actionDefault->setProperty("ActionType", ACTION_DEFAULT);
    m_ui->actionFootballField->setProperty("ActionType", ACTION_FOOTBALL_FIELD);
    m_ui->actionForest->setProperty("ActionType", ACTION_FOREST);
    m_ui->actionLake->setProperty("ActionType", ACTION_LAKE);
    m_ui->actionSee->setProperty("ActionType", ACTION_SEA);
    m_ui->actionNode_Camera->setProperty("ActionType", ACTION_NODE_CAMERA);
    m_ui->actionSave_Layout->setProperty("ActionType", ACTION_SAVE_LAYOUT);
    m_ui->actionEnglish_Chinese->setProperty("ActionType", ACTION_LANGUAGE);
    m_ui->actionSet_NASLOC->setProperty("ActionType", ACTION_SET_NASLOC);
    m_ui->actionSet_ZENCACHE->setProperty("ActionType", ACTION_ZENCACHE);

}

void ZenoMainWindow::screenShoot() 
{
    QString path = QFileDialog::getSaveFileName(
        nullptr, tr("Path to Save"), "",
        tr("PNG images(*.png);;JPEG images(*.jpg);;BMP images(*.bmp);;EXR images(*.exr);;HDR images(*.hdr);;"));
    QString ext = QFileInfo(path).suffix();
    if (!path.isEmpty()) {
        Zenovis::GetInstance().getSession()->do_screenshot(path.toStdString(), ext.toStdString());
    }
}

void ZenoMainWindow::setActionIcon(QAction *action) 
{
    if (!action->isCheckable() || !action->isChecked()) 
    {
        action->setIcon(QIcon());
    }
    if (action->isChecked()) 
    {
        action->setIcon(QIcon("://icons/checked.png"));
    }
}

void ZenoMainWindow::onDockSwitched(DOCK_TYPE type)
{
    ZenoDockWidget *pDock = qobject_cast<ZenoDockWidget *>(sender());
    switch (type)
    {
        case DOCK_EDITOR: {
            ZenoGraphsEditor *pEditor2 = new ZenoGraphsEditor(this);
            pEditor2->resetModel(zenoApp->graphsManagment()->currentModel());
            pDock->setWidget(type, pEditor2);
            break;
        }
        case DOCK_VIEW: {
            //complicated opengl framework.
            DisplayWidget* view = new DisplayWidget;
            pDock->setWidget(type, view);
            break;
        }
        case DOCK_NODE_PARAMS: {
            ZenoPropPanel *pWidget = new ZenoPropPanel;
            pDock->setWidget(type, pWidget);
            break;
        }
        case DOCK_NODE_DATA: {
            ZenoSpreadsheet *pWidget = new ZenoSpreadsheet;
            pDock->setWidget(type, pWidget);
            break;
        }
        case DOCK_LOG: {
            ZPlainLogPanel* pPanel = new ZPlainLogPanel;
            pDock->setWidget(type, pPanel);
            break;
        }
        case DOCK_LIGHTS: {
            ZenoLights* pPanel = new ZenoLights;
            pDock->setWidget(type, pPanel);
            break;
        }
    }
    pDock->setObjectName(uniqueDockObjName(type));
}

bool ZenoMainWindow::saveQuit() {
    auto pGraphsMgm = zenoApp->graphsManagment();
    ZASSERT_EXIT(pGraphsMgm, true);
    IGraphsModel *pModel = pGraphsMgm->currentModel();
    if (!zeno::envconfig::get("OPEN") /* <- don't annoy me when I'm debugging via ZENO_OPEN */ && pModel && pModel->isDirty()) {
        QMessageBox msgBox(QMessageBox::Question, tr("Save"), tr("Save changes?"), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, this);
        QPalette pal = msgBox.palette();
        pal.setBrush(QPalette::WindowText, QColor(0, 0, 0));
        msgBox.setPalette(pal);
        int ret = msgBox.exec();
        if (ret & QMessageBox::Yes) {
            save();
        }
        if (ret & QMessageBox::Cancel) {
            return false;
        }
    }
    pGraphsMgm->clear();
    //clear timeline info.
    resetTimeline(TIMELINE_INFO());
    return true;
}

void ZenoMainWindow::save()
{
    auto pGraphsMgm = zenoApp->graphsManagment();
    ZASSERT_EXIT(pGraphsMgm);
    IGraphsModel *pModel = pGraphsMgm->currentModel();
    if (pModel) {
        QString currFilePath = pModel->filePath();
        if (currFilePath.isEmpty())
            return saveAs();
        saveFile(currFilePath);
    }
}

bool ZenoMainWindow::saveFile(QString filePath)
{
    IGraphsModel* pModel = zenoApp->graphsManagment()->currentModel();
    APP_SETTINGS settings;
    settings.timeline = timelineInfo();
    zenoApp->graphsManagment()->saveFile(filePath, settings);
    recordRecentFile(filePath);
    return true;
}

bool ZenoMainWindow::inDlgEventLoop() const {
    return m_bInDlgEventloop;
}

void ZenoMainWindow::setInDlgEventLoop(bool bOn) {
    m_bInDlgEventloop = bOn;
}

TIMELINE_INFO ZenoMainWindow::timelineInfo()
{
    TIMELINE_INFO info;
    ZASSERT_EXIT(m_pTimeline, info);
    info.bAlways = m_pTimeline->isAlways();
    info.beginFrame = m_pTimeline->fromTo().first;
    info.endFrame = m_pTimeline->fromTo().second;
    return info;
}

void ZenoMainWindow::resetTimeline(TIMELINE_INFO info)
{
    m_pTimeline->setAlways(info.bAlways);
    m_pTimeline->initFromTo(info.beginFrame, info.endFrame);
}

void ZenoMainWindow::onFeedBack()
{
    /*
    ZFeedBackDlg dlg(this);
    if (dlg.exec() == QDialog::Accepted)
    {
        QString content = dlg.content();
        bool isSend = dlg.isSendFile();
        if (isSend)
        {
            IGraphsModel *pModel = zenoApp->graphsManagment()->currentModel();
            if (!pModel) {
                return;
            }
            QString strContent = ZsgWriter::getInstance().dumpProgramStr(pModel);
            dlg.sendEmail("bug feedback", content, strContent);
        }
    }
    */
}

void ZenoMainWindow::clearErrorMark()
{
    //clear all error mark at every scene.
    auto graphsMgm = zenoApp->graphsManagment();
    IGraphsModel* pModel = graphsMgm->currentModel();
    if (!pModel) {
        return;
    }
    const QModelIndexList& lst = pModel->subgraphsIndice();
    for (const QModelIndex& idx : lst)
    {
        ZenoSubGraphScene* pScene = qobject_cast<ZenoSubGraphScene*>(graphsMgm->gvScene(idx));
        if (!pScene) {
            pScene = new ZenoSubGraphScene(graphsMgm);
            graphsMgm->addScene(idx, pScene);
            pScene->initModel(idx);
        }

        if (pScene) {
            pScene->clearMark();
        }
    }
}

void ZenoMainWindow::saveAs() {
    DlgInEventLoopScope;
    QString path = QFileDialog::getSaveFileName(this, "Path to Save", "", "Zeno Graph File(*.zsg);; All Files(*);;");
    if (!path.isEmpty()) {
        saveFile(path);
    }
}

QString ZenoMainWindow::getOpenFileByDialog() {
    DlgInEventLoopScope;
    const QString &initialPath = "";
    QFileDialog fileDialog(this, tr("Open"), initialPath, "Zeno Graph File (*.zsg)\nAll Files (*)");
    fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    fileDialog.setFileMode(QFileDialog::ExistingFile);
    if (fileDialog.exec() != QDialog::Accepted)
        return "";

    QString filePath = fileDialog.selectedFiles().first();
    return filePath;
}

void ZenoMainWindow::onNodesSelected(const QModelIndex &subgIdx, const QModelIndexList &nodes, bool select) {
    //dispatch to all property panel.
    auto docks2 = findChildren<ZTabDockWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (ZTabDockWidget* dock : docks2) {
        dock->onNodesSelected(subgIdx, nodes, select);
    }
}

void ZenoMainWindow::onPrimitiveSelected(const std::unordered_set<std::string>& primids) {
    //dispatch to all property panel.
    auto docks = findChildren<ZenoDockWidget *>(QString(), Qt::FindDirectChildrenOnly);
    for (ZenoDockWidget *dock : docks) {
        dock->onPrimitiveSelected(primids);
    }
}

void ZenoMainWindow::updateLightList() {
    auto docks = findChildren<ZenoDockWidget *>(QString(), Qt::FindDirectChildrenOnly);
    for (ZenoDockWidget *dock : docks) {
        dock->newFrameUpdate();
    }
}
