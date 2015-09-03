/////////////////////////////////////////////////////////////////
/// @file      TreeView.cpp
/// @author    Chris L Baker (clb) <chris@chimail.net>
/// @date      2014.05.20
///
/// @attention Copyright (C) 2014
/// @attention All rights reserved
/////////////////////////////////////////////////////////////////

#include "TreeView.h"
#include "QOSGWidget.h"

#include <QtGui/QTreeView>
#include <QtGui/QActionGroup>
#include <QtGui/QCheckBox>

#include <iostream>

namespace d3
{

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
TreeView::TreeView() :
    QTreeView(),
    m_pOsgWidget(nullptr),
    m_pModel(nullptr),
    m_mutex()
{
    // connect for clicks to show/hide stuff
    QObject::connect(this,
                     SIGNAL(clicked(QModelIndex)),
                     this,
                     SLOT(clicked(QModelIndex)));

    // connet to expand to resize the column width
    QObject::connect(this,
                     SIGNAL(expanded(QModelIndex)),
                     this,
                     SLOT(expanded(QModelIndex)));

    // connect to collaps to resize the column width
    QObject::connect(this,
                     SIGNAL(collapsed(QModelIndex)),
                     this,
                     SLOT(collapsed(QModelIndex)));

    // make the qmodel
    m_mutex.lock();
    m_pModel = new QStandardItemModel();

    // add the model
    setModel( m_pModel );
    header()->hide();
    setAllColumnsShowFocus(true);
    m_mutex.unlock();
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
TreeView::~TreeView()
{
    m_pModel->clear();
    reset();
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void TreeView::setOsgWidget(QOSGWidget* widget)
{
    // make sure we have a valid widget
    if ( nullptr == widget ) return;

    // Hold the widget
    m_pOsgWidget = widget;

    // get the lock
    m_mutex.lock();

    // setup the top level entry for the root node
    d3DisplayItem* topEntry(new d3DisplayItem("All Displayed Items",
                                              "All Displayed Items",
                                              m_pOsgWidget->getRootGroup(),
                                              [](d3DisplayItem*){}));
    m_pModel->appendRow(topEntry);

    // set to accomodate this width
    resizeColumnToContents(0);

    // expand the first depth
    expandToDepth(0);

    // release the lock
    m_mutex.unlock();
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
bool TreeView::add(const std::string& name,
                   const osg::ref_ptr<osg::Node> node,
                   const bool& showNode,
                   const bool& addToDisplay,
                   std::function<void(d3DisplayItem*)>&& clickCallback,
                   std::function<void(d3DisplayItem*)>&& creationCallback)
{
    // by default enable the node
    static const bool enableNode(true);

    // make sure the osg widget has been set
    if ( (nullptr != m_pOsgWidget) )
    {
        // pass this along to the internal adder which does the adds based on items
        // and give it the top level item in the model
        m_mutex.lock();
        d3DisplayItem* myItem(static_cast<d3DisplayItem*>(m_pModel->item(0)));
        bool rv( add(name,
                     name,
                     node,
                     std::move(clickCallback),
                     std::move(creationCallback),
                     enableNode,
                     showNode,
                     addToDisplay,
                     myItem) );
        m_mutex.unlock();

        // we're done
        return rv;
    }

    // we have no widget - we have failed to add to the non-existent widget
    return false;
};

/////////////////////////////////////////////////////////////////
/////////////// SLOTS //////////////////////////////////////////
///////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void TreeView::clicked(const QModelIndex& index)
{
    // make sure we have a valid osg and the window is visible
    if ( (nullptr != m_pOsgWidget) )
    {
        // lock the model view
        m_mutex.lock();

        // get the item from the menu index
        d3DisplayItem* item = static_cast<d3DisplayItem*>(m_pModel->itemFromIndex(index));

        // make sure we have the item
        if ( not item )
            throw std::runtime_error("Item not retrieved");

        // lock osg and set the node mask based on the checked state
        m_pOsgWidget->lock();
        if ( Qt::Checked == item->checkState() )
        {
            //item->getNode()->setNodeMask(~0);
            if ( item->isEnabled() )
                item->getNode()->setNodeMask(item->getPriorNodeMask());
            else
                item->getNode()->setNodeMask(0);
        }
        else
        {
            if ( item->isEnabled() )
                item->setPriorNodeMask(item->getNode()->getNodeMask());
            item->getNode()->setNodeMask(0);
        }

        // now we have to recursively process the children to have them match
        // this entry's checked state
        for ( int ii(0) ; ii<item->rowCount() ; ++ii )
            updateChildren(static_cast<d3DisplayItem*>(item->child(ii)),
                           Qt::Checked == item->checkState());

        // unlock osg and the model view
        m_pOsgWidget->unlock();
        m_mutex.unlock();

        // run any registered function
        item->runClickCallback(item);
    }
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void TreeView::expanded(const QModelIndex& index)
{
    resizeColumnToContents(0);
    auto item( static_cast<d3DisplayItem*>(m_pModel->itemFromIndex(index)) );
    if ( item ) item->runClickCallback(item);
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void TreeView::collapsed(const QModelIndex& index)
{
    resizeColumnToContents(0);
    auto item( static_cast<d3DisplayItem*>(m_pModel->itemFromIndex(index)) );
    if ( item ) item->runClickCallback(item);
};

/////////////////////////////////////////////////////////////////
///////////// PRIVATES /////////////////////////////////////////
///////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
bool TreeView::add(const std::string& name,
                   const std::string& path,
                   const osg::ref_ptr<osg::Node> node,
                   std::function<void(d3DisplayItem*)>&& clickCallback,
                   std::function<void(d3DisplayItem*)>&& creationCallback,
                   const bool& enableNode,
                   const bool& showNode,
                   const bool& addToDisplay,
                   d3DisplayItem* myParent)
{
    static const std::string splitIndicator("::");

    // get the name split point
    size_t nameSplit( name.find(splitIndicator) );

    // see if we have children
    if ( std::string::npos != nameSplit )
    {
        // the first part is the parent's name
        std::string pre( name.substr(0, nameSplit) );

        // the second part is the children
        std::string post( name.substr(nameSplit + splitIndicator.size(), name.size()) );

        // call the add parent
        return addParent(pre,
                         post,
                         path,
                         node,
                         std::move(clickCallback),
                         std::move(creationCallback),
                         enableNode,
                         showNode,
                         addToDisplay,
                         myParent);
    }

    // this must be a leaf node... do we already have this child?
    d3DisplayItem* entry(findChild(myParent, name));

    // see if we need to create a new entry
    if ( not entry )
        return createNewEntry(name,
                              path,
                              node,
                              std::move(clickCallback),
                              std::move(creationCallback),
                              enableNode,
                              showNode,
                              addToDisplay,
                              myParent);
    return replaceNode(entry,
                       node,
                       std::move(clickCallback),
                       enableNode,
                       addToDisplay,
                       myParent);
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
bool TreeView::createNewEntry(const std::string& name,
                              const std::string& path,
                              const osg::ref_ptr<osg::Node> node,
                              std::function<void(d3DisplayItem*)>&& clickCallback,
                              std::function<void(d3DisplayItem*)>&& creationCallback,
                              const bool& enableNode,
                              const bool& showNode,
                              const bool& addToDisplay,
                              d3DisplayItem* myParent)
{
    // create the entry for the item model
    d3DisplayItem* entry( new d3DisplayItem(name,
                                            path,
                                            node,
                                            std::move(clickCallback)) );
    entry->setEnabled(enableNode);

    // add this entry to the item model
    m_mutex.lock();
    myParent->appendRow(entry);

    // set to accomodate this width
    resizeColumnToContents(0);

    // conditionally add the node to the osg tree
    if ( addToDisplay )
    {
        m_pOsgWidget->lock();
        myParent->getNode()->asGroup()->addChild(node);
        m_pOsgWidget->unlock();
    }

    // conditionally show the node
    if ( not showNode )
    {
        entry->setCheckState(Qt::Unchecked);
        clicked(m_pModel->indexFromItem(entry));
    }

    // call the creation callback
    creationCallback(entry);
    
    m_mutex.unlock();

    return true;
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
bool TreeView::replaceNode(d3DisplayItem* entry,
                           const osg::ref_ptr<osg::Node> node,
                           std::function<void(d3DisplayItem*)>&& clickCallback,
                           const bool& enableNode,
                           const bool& addToDisplay,
                           d3DisplayItem* myParent)
{
    // get the lock
    m_pOsgWidget->lock();

    // set the new node mask to match the old one
    node->setNodeMask(entry->getNode()->getNodeMask());

    // conditionally remove this entry child from the display
    if ( addToDisplay )
        myParent->getNode()->asGroup()->removeChild(entry->getNode());

    // now lock the model view
    m_mutex.lock();

    // set the node and enabled flags
    entry->setNode(node);
    entry->setEnabled(enableNode);

    // unlock the model view
    m_mutex.unlock();

    // now the entry is ready, so conditinoally add it back as a child to my
    // parent 
    if ( addToDisplay )
        myParent->getNode()->asGroup()->addChild(entry->getNode());

    // unlock osg
    m_pOsgWidget->unlock();

    return true;
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
bool TreeView::addParent(const std::string& parentName,
                         const std::string& childName,
                         const std::string& path,
                         const osg::ref_ptr<osg::Node> node,
                         std::function<void(d3DisplayItem*)>&& clickCallback,
                         std::function<void(d3DisplayItem*)>&& creationCallback,
                         const bool& enableNode,
                         const bool& showNode,
                         const bool& addToDisplay,
                         d3DisplayItem* myParent)
{
    // see if the parent we are about to add as an offspring already exists
    d3DisplayItem* entry(findChild(myParent, parentName));

    // if it's null, we must create it
    if ( nullptr == entry )
    {
        // here is the entry named by the firstPart as a group
        entry = new d3DisplayItem(parentName,
                                  path,
                                  new osg::Group(),
                                  std::move(clickCallback));
        entry->setEnabled(true);

        // lock
        m_mutex.lock();

        // add the node to the osg tree
        m_pOsgWidget->lock();
        myParent->getNode()->asGroup()->addChild(entry->getNode());
        m_pOsgWidget->unlock();

        // add this entry to the item model
        myParent->appendRow(entry);

        // set to accomodate this width
        resizeColumnToContents(0);

        // call the creation callback as we have created this thing
        creationCallback(entry);

        // unlock
        m_mutex.unlock();
    }

    // make sure this entry is a group
    osg::Group* group( entry->getNode()->asGroup() );
    if ( group )
    {
        // now we can recurse with this item
        return add(childName,
                   path,
                   node,
                   std::move(clickCallback),
                   std::move(creationCallback),
                   enableNode,
                   showNode,
                   addToDisplay,
                   entry);
    }

    // it wasn't a group...
    std::cerr << "ERROR - " << parentName << " already exists as a non-group ("
              << entry->getNode()->className() << ")" << std::endl
              << " --> YOu can't added to an already-added thing that isn't a group" << std::endl;
    return false;
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
TreeView::d3DisplayItem* TreeView::findChild(const d3DisplayItem* myParent,
                                             const std::string& name)
{
    // go through the list of rows and see if we can find an entry with "name"
    for ( int ii=0 ; ii<myParent->rowCount() ; ++ii )
    {
        d3DisplayItem* ch = static_cast<d3DisplayItem*>(myParent->child(ii));
        if ( ch && (ch->getName() == name) )
            return ch;
    }
    return nullptr;
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void TreeView::updateChildren(d3DisplayItem* item,
                              const bool& checked)
{
    item->setEnabled(checked);

    uint ii(0);
    while ( true )
    {
        d3DisplayItem* child( static_cast<d3DisplayItem*>(item->child(ii)) );
        if ( nullptr == child ) break;
        if ( Qt::Checked == item->checkState() )
        {
            updateChildren(child, checked);
        }
        ++ii;
    }

    if ( checked && Qt::Checked == item->checkState() )
    {
        item->getNode()->setNodeMask(item->getPriorNodeMask());
    }
    else if ( Qt::Checked == item->checkState() )
    {
        item->setPriorNodeMask(item->getNode()->getNodeMask());
        item->getNode()->setNodeMask(0);
    }
    else
    {
        item->getNode()->setNodeMask(0);
    }
};

} // namespace d3
