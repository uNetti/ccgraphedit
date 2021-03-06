
#pragma once

#include "mainwindow.h"
#include "exporter.h"
#include "importer.h"
#include "deviceframe.h"
#include <QObject>
#include <QTreeWidgetItem>
#include <QTreeWidget>
#include "ccTypeInfo.h"
#include <map>

class QWidget;
class QTreeWidget;
namespace cocos2d {
    class Node;
    class StreamFormatted;
}
class NodeItem;
class Exporter;

class INodeDriver
{
public:

    // returns the name of this node
    virtual const char* Name() const = 0;

    // returns the id of this driver
    virtual uint32_t Id() const = 0;

    // pushes edits to the node
    virtual void Push() = 0;

    // update widget from node if changed
    virtual void Update(bool force = false) = 0;

    // the row item in the property tree
    virtual QTreeWidgetItem* Item() const = 0;

    // the property widget associated with this driver.
    virtual QWidget* Widget() const = 0;

    // sets the node associated with this driver
    virtual void SetNode(cocos2d::Node* node) = 0;

    // the node associated with this driver
    virtual cocos2d::Node* Node() const = 0;

    // called whenever the tool shows properties for a node
    // it needs to create the widget and connect things.
    virtual void SetupWidget(QTreeWidget* tree) = 0;

    // called to destroy the widget for this driver
    virtual void DestroyWidget() = 0;

    // clones this driver except for the widgets
    virtual INodeDriver* Clone() = 0;

    // exports the value type to the stream via the exporter (invokes correct specialization based on type)
    virtual bool Export(cocos2d::StreamFormatted& stream, Exporter* exporter) = 0;

    // imports the value type from the stream via importer (invokes correct specialization based on type)
    virtual bool Import(cocos2d::StreamFormatted& stream, Importer* importer) = 0;
};

template <class widgetT, class nodeT, typename varT>
class NodeDriverT
    : public INodeDriver
{
public:

    typedef NodeDriverT<widgetT, nodeT, varT> instance_type;

    typedef std::function<void(nodeT*, const varT&)> setter_type;
    typedef std::function<void(nodeT*, varT&)> getter_type;

    NodeDriverT(void (*setter)(nodeT*, const varT&), void (*getter)(nodeT*, varT&), const char* name)
        : mId(cocos2d::fnv1_32(name))
        , mNode(nullptr)
        , mSetter(setter)
        , mGetter(getter)
        , mName(name)
        , mItem(nullptr)
        , mWidget(nullptr)
        , mIncrement(1)
        , mbPercentageToWorld(false)
    {}

    NodeDriverT(const setter_type& setter, const getter_type& getter, const char* name)
        : mId(cocos2d::fnv1_32(name))
        , mNode(nullptr)
        , mSetter(setter)
        , mGetter(getter)
        , mName(name)
        , mItem(nullptr)
        , mWidget(nullptr)
        , mIncrement(1)
        , mbPercentageToWorld(false)
    {}

    // this has to be done each time it is added to the tree since the tree takes
    // ownership of it, and there is no way to get it back, so it is recreated.
    void SetupWidget(QTreeWidget* tree)
    {
        QTreeWidgetItem* parent = tree->invisibleRootItem();

        mItem = new QTreeWidgetItem;
        mItem->setText(0, mName.c_str());
        parent->addChild(mItem);

        mWidget = new widgetT(tree);
        mWidget->SetIncrement(mIncrement);
        mWidget->setProperty("node", QVariant((qlonglong)mNode));
        mItem->setData(0, Qt::UserRole, QVariant((qlonglong)mWidget));

        tree->setItemWidget(mItem, 1, mWidget);

        // set the default value
        Update(true);

        QObject::connect(mWidget, SIGNAL(widgetChanged(QWidget*)), cocos2d::MainWindow::instance(), SLOT(pushWidget(QWidget*)));
    }

    void DestroyWidget()
    {
        // we do not own this widget so we just delete our reference
        mWidget = 0;
    }

    // templated creator method to instantiate drivers
    template <class componentT = IComponent>
    static NodeDriverT<widgetT, nodeT, varT>* create(const char* name, void (*setter)(nodeT*, const varT&), void (*getter)(nodeT*, varT&), float increment = 1, bool convert = false)
    {
        instance_type* driver = new instance_type(setter, getter, name);
        driver->SetIncrement(increment);
        driver->SetConvertPercentageToWorld(convert);
        return driver;
    }

    // clones this driver except for the widgets
    INodeDriver* Clone()
    {
        instance_type* driver = new instance_type(mSetter, mGetter, Name());
        driver->SetIncrement(mIncrement);
        driver->SetConvertPercentageToWorld(mbPercentageToWorld);
        return driver;
    }

    // specify the increment for spin boxes
    void SetIncrement(float increment)
    {
        mIncrement = increment;
    }

    // returns the id of this driver
    uint32_t Id() const
    {
        return mId;
    }

    // update cached value and push to node
    void Push()
    {
        if (mWidget)
            mValue = mWidget->Value();
        varT value = PercentageToWorld(mValue);
        mSetter(mNode, value);
    }

    // update cached value and widget from node
    void Update(bool force = false)
    {
        varT value = mValue;
        if (mGetter)
            mGetter(mNode, value);
        value = WorldToPercentage(value);
        varT lastValue = mValue;
        mValue = value;
        if (mWidget && (force || mWidget->Compare(lastValue, value)))
            mWidget->SetValue(value);
    }

    QTreeWidgetItem* Item() const
    {
        return mItem;
    }

    QWidget* Widget() const
    {
        return mWidget;
    }

    void SetNode(cocos2d::Node* node)
    {
        nodeT* typedNode = dynamic_cast<nodeT*>(node);
        Q_ASSERT(nullptr != typedNode);
        mNode = typedNode;
    }

    cocos2d::Node* Node() const
    {
        return mNode;
    }

    const char* Name() const
    {
        return mName.c_str();
    }

    void SetValue(const varT& value)
    {
        mSetter(mNode, PercentageToWorld(value));
        mValue = value;
    }

    void SetConvertPercentageToWorld(bool convert)
    {
        mbPercentageToWorld = convert;
    }

    bool Export(cocos2d::StreamFormatted& stream, Exporter* exporter)
    {
        return exporter->ExportProperty(stream, &mValue);
    }

    bool Import(cocos2d::StreamFormatted &stream, Importer *importer)
    {
        return importer->ImportProperty(stream, &mValue);
    }

#define ConvertDefault(T) \
    T PercentageToWorld(const T& var) { return var; } \
    T WorldToPercentage(const T& var) { return var; }

    ConvertDefault(bool);
    ConvertDefault(int);
    ConvertDefault(float);
    ConvertDefault(cocos2d::Point);
    ConvertDefault(cocos2d::Size);
    ConvertDefault(cocos2d::_ccColor3B);
    ConvertDefault(std::string);

protected:

    // hash of name
    uint32_t mId;

    // cached value for this property
    // *note this variable is explicitly not initialized in the constructor*.
    varT mValue;

    // node that has this property
    nodeT* mNode;

    // setter for the node
    setter_type mSetter;

    // getter for the node
    getter_type mGetter;

    // name of the property
    std::string mName;

    // container item in property tree widget
    QTreeWidgetItem* mItem;

    // property display/edit widget for column 1 in property tree widget
    widgetT* mWidget;

    // (for spin boxes) amount to increment/decrement
    float mIncrement;

    // if set, coords are stored as percentages but converted to/from
    // world coordinates around the get/set routines
    bool mbPercentageToWorld;
};
