
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "myqglwidget.h"
#include "fileutil.h"
#include "mysceneeditor.h"
#include "dialogimportccb.h"
#include "nodeitem.h"
#include "widgetpoint.h"
#include "componentnode.h"
#include "componentsprite.h"

#include "cocos2d.h"
#include "CCFileUtils.h"
#include "CCClassRegistry.h"
#include "CCBReader/CCBReader.h"
#include "CCBReader/CCNodeLoaderLibrary.h"

#include <QStandardItemModel>
#include <QMenuBar>
#include <QAbstractListModel>
#include <QLineEdit>
#include <QCheckBox>
#include <QMessageBox>

USING_NS_CC;
USING_NS_CC_EXT;

namespace
{
    const uint32_t kNodeDriverPosition = fnv1_32("position");
};

IMPLEMENT_SINGLETON(MainWindow)

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , mQGLWidget(nullptr)
    , mSelectedNode(nullptr)
{
    ui->setupUi(this);

    // register the components
    RegisterComponent(Node::kClassId, new ComponentNode);
    RegisterComponent(Sprite::kClassId, new ComponentSprite);

    // connect any signals and slots
    connect(MySceneEditor::instance(), SIGNAL(positionChanged(Node*, Point&)), this, SLOT(setNodePosition(Node*,Point&)));

    // add our cocos2dx opengl widget to the splitter in the correct place
    mQGLWidget = new MyQGLWidget;
    mQGLWidget->show(); // this must come before adding to the graph since it initializes cocos2d.
    ui->splitter->insertWidget(1, mQGLWidget);

    if (ui->hierarchy)
    {
        QStringList labels;
        labels << "Scene Graph";
        ui->hierarchy->setHeaderLabels(labels);

        connect(ui->hierarchy->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)), this, SLOT(selectNode()));
    }

    if (ui->properties)
    {
        QStringList labels;
        labels << "Properties" << "Value";
        ui->properties->setHeaderLabels(labels);
    }

    // Add a path for our test sprite
    FileUtils::sharedFileUtils()->addSearchPath("../../../../../cocos2d/template/multi-platform-cpp/proj.ios");
    FileUtils::sharedFileUtils()->addSearchPath("/Users/jgraham/dev_qtTest/resources/images/frames");

    Sprite* frame = Sprite::create("frame-ipad.png");
    if (frame)
    {
        Node* scene = Director::sharedDirector()->getRunningScene();
        Node* root = Node::create();
        AddNode(scene, root, "root");
        MySceneEditor::instance()->SetRootNode(root);
        scene->addChild(frame);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

Ui::MainWindow* MainWindow::UI()
{
    return ui;
}

void MainWindow::AddFiles(const char* root, const char* path, bool directory)
{
    if (directory)
        FileUtils::sharedFileUtils()->addSearchPath(path);
}

void MainWindow::AddNode(Node* parent, Node* node, const char* nodeName)
{
    tNodeToNodeItemMap::iterator it = mNodeToNodeItemMap.find(node);
    if (it != mNodeToNodeItemMap.end())
    {
        QMessageBox::information(nullptr, QString("Error"), QString("Node cannot be added twice"), QMessageBox::Ok);
        return;
    }

    if (parent)
        parent->addChild(node);

    if (ui->hierarchy)
    {
        // Find the parent so that we can append to it in the tree view
        QTreeWidgetItem* parentItem;
        tNodeToNodeItemMap::iterator it = mNodeToNodeItemMap.find(parent);
        if (it == mNodeToNodeItemMap.end())
        {
            parentItem = ui->hierarchy->invisibleRootItem();
        }
        else
        {
            NodeItem* item = (*it).second;
            parentItem = item->SceneItem();
        }

        String* className = CCClassRegistry::instance()->getClassName(node->classId());

        NodeItem* item = new NodeItem;

        QTreeWidgetItem* sceneItem = new QTreeWidgetItem;
        sceneItem->setText(0, QString(className->getCString()));

        item->SetNode(node);
        item->SetSceneItem(sceneItem);

        parentItem->addChild(sceneItem);

        mNodeToNodeItemMap.insert(tNodeToNodeItemMap::value_type(node, item));
    }
}

void MainWindow::RegisterComponent(uint32_t classId, ComponentBase* component)
{
    mClassToComponentMap.insert(tClassToComponentMap::value_type(classId, component));
}

ComponentBase* MainWindow::FindComponent(uint32_t classId)
{
    tClassToComponentMap::iterator it = mClassToComponentMap.find(classId);
    return it == mClassToComponentMap.end() ? nullptr : (*it).second;
}

//
// Public Slots
//

void MainWindow::importCCB()
{
    DialogImportCCB* dialog = new DialogImportCCB(this);
    dialog->setModal(true);
    dialog->show();
    dialog->exec();

    FileUtil::EnumerateDirectoryT(dialog->ccbPath().toUtf8(), 0, this, &MainWindow::AddFiles);
    FileUtil::EnumerateDirectoryT(dialog->resourcesPath().toUtf8(), 0, this, &MainWindow::AddFiles);

    CCBReader* ccbReader = new CCBReader(NodeLoaderLibrary::sharedNodeLoaderLibrary());
    Node* node = ccbReader->readNodeGraphFromFile(dialog->ccbPath().toUtf8());
    if (node)
    {
        AddNode(0, node, "");
    }
}

void MainWindow::selectNode()
{
    Node* selectedNode = GetSelectedNodeInHierarchy();
    MySceneEditor::instance()->SetSelectedNode(selectedNode);
    SetPropertyViewForNode(selectedNode, mSelectedNode);
    mSelectedNode = selectedNode;
}

void MainWindow::setNodePosition(Node* node, Point& position)
{
    ComponentBase* plugin = FindComponent(node->classId());
    if (plugin)
    {
        INodeDriver* driver = plugin->FindDriverByHash(kNodeDriverPosition);
        if (driver)
        {
            QWidget* widget = driver->Widget();
            widgetPoint* wp = dynamic_cast<widgetPoint*>(widget);
            if (wp)
            {
                wp->SetValue(position, true);
            }
        }
    }
}

void MainWindow::pushWidget(QWidget* widget)
{
    QVariant var = widget->property("node");
    Node* node = (Node*)var.toLongLong();
    if (node)
    {
        ComponentBase* plugin = FindComponent(node->classId());
        if (plugin)
        {
            INodeDriver* driver = plugin->FindDriverByWidget(widget);
            if (driver)
            {
                driver->Push();
            }
        }
    }
}

//
// Toolbar Actions
//

void MainWindow::on_actionCCSprite_triggered()
{
    Size size = Director::sharedDirector()->getWinSize();

    Node* parent = GetSelectedNodeInHierarchy();
    if (!parent)
        parent = MySceneEditor::instance()->GetRootNode();

    Sprite* sprite = Sprite::create("Icon-144.png");
    if (sprite)
    {
        sprite->setPosition(ccp(.5f * size.width, .5f * size.height));
        AddNode(parent, sprite, "Sprite");
    }
}

void MainWindow::on_actionCCNode_triggered()
{
    Size size = Director::sharedDirector()->getWinSize();

    Node* parent = GetSelectedNodeInHierarchy();

    Node* node = Node::create();
    if (node)
    {
        node->setPosition(ccp(.5f * size.width, .5f * size.height));
        AddNode(parent, node, "Node");
    }
}

//
// Protected Methods
//

Node* MainWindow::GetSelectedNodeInHierarchy()
{
    QList<QTreeWidgetItem*> nodes = ui->hierarchy->selectedItems();
    if (nodes.empty())
        return nullptr;

    QTreeWidgetItem* widget = nodes.front();
    QVariant var = widget->data(0, Qt::UserRole);
    NodeItem* item = (NodeItem*)var.toLongLong();

    return item->GetNode();
}

void MainWindow::SetPropertyViewForNode(Node* node, Node* oldNode)
{
    if (ui->properties)
    {
        QTreeWidgetItem* root = ui->properties->invisibleRootItem();

        tNodeToNodeItemMap::iterator it = mNodeToNodeItemMap.find(node);
        if (it == mNodeToNodeItemMap.end())
        {
            QMessageBox::information(nullptr, QString("Error"), QString("Node cannot be found in the map"), QMessageBox::Ok);
            return;
        }

        // remove all children of the root node
        while (root->childCount())
            root->takeChild(0);

        // destroy all for last node being displayed
        if (oldNode)
        {
            ComponentBase* lastPlugin = FindComponent(oldNode->classId());
            if (lastPlugin)
                lastPlugin->DestroyAll();
        }

        // Don't allow editing of the nodes above/next to root
        if (!MySceneEditor::instance()->IsChildOfRoot(node))
            return;

        ComponentBase* plugin = FindComponent(node->classId());
        if (plugin)
        {
            plugin->Populate(ui->properties, root, node);
        }
        else
        {
            QMessageBox::information(nullptr, QString("Error"), QString("Component cannot be found to populate node"), QMessageBox::Ok);
        }
    }
}
