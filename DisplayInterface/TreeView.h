/////////////////////////////////////////////////////////////////
/// @file      TreeView.h
/// @author    Chris L Baker (clb) <chris@chimail.net>
/// @date      2015.06.16
///
/// @attention Copyright (C) 2015
/// @attention
/// @attention
/// @attention All rights reserved
/// @attention
/////////////////////////////////////////////////////////////////

#pragma once

#include <QtGui/QApplication>
#include <QtGui/QtGui>
#include <QtGui/QSplitter>

#include <osg/Node>
#include <mutex>
#include <functional>

namespace d3
{

class QOSGWidget;

/////////////////////////////////////////////////////////////////
/// @brief   Class to hold the tree view - this makes it useful in other
///          contexts as a sister class to the QOSGWidget, this can
///          automatically setup a vew control which allows hierarchical
///          displays with simple syntax
/////////////////////////////////////////////////////////////////
class TreeView : public QTreeView
{
    /// standard q-craziness
    Q_OBJECT;

  public:

    /// @brief   Constructor
    TreeView();

    /// @brief   Destructor
    virtual ~TreeView();

    /// @brief   Method to set the osg widget
    /// @param   widget We're controlling the display items in this widget, so
    ///          the "add()" method adds them to the widget, and we control if
    ///          they are displayed or not
    void setOsgWidget(QOSGWidget* widget);

    /// @brief   Method to add an object to the display
    /// @param   name The name associated with the node - used to provide some
    ///          text by the checkbox that will be used to control the display
    /// @param   showNode A flag to indicate if the node should be shown
    ///          initially or not
    /// @param   addToDisplay A flag to indicate if we should add this node to
    ///          the display graph. It's possible to have already added a node
    ///          to the display graph and you just want the tree-view to manage
    ///          that node. Setting this to false will not alter the osg display
    ///          graph in any way, assuming that's been done externally.
    /// @param   clickCallback A function that can be called from the user whenever
    ///          this item is clicked
    bool add(const std::string& name,
             const osg::ref_ptr<osg::Node> node,
             const bool& showNode,
             const bool& addToDisplay = true,
             std::function<void()>&& clickCallback = [](){});

  public Q_SLOTS:

    /// @brief   Method to call when the frame is clicked
    void clicked(const QModelIndex& index);

    /// @brief   Method to reset the column width when something is expanded
    void expanded(const QModelIndex& index);
    
    /// @brief   Method to reset the column width when something is collapsed
    void collapsed(const QModelIndex& index);

  private:

    /////////////////////////////////////////////////////////////////
    /// @brief   Override the standard qitem for the tree view so we can hold an
    ///          osg node and name of the item
    /////////////////////////////////////////////////////////////////
    class d3DisplayItem : public QStandardItem
    {
      public:
        /// @brief   Construct with a name and node
        /// @param   name The name of the node
        /// @param   node The node to add to this item
        /// @param   func The func to run at the end of the "clicked" event for
        ///          an item
        d3DisplayItem(const std::string& name,
                      const osg::ref_ptr<osg::Node> node,
                      std::function<void()>&& func) :
            QStandardItem(QString(name.c_str())),
            m_name(name),
            m_node(node),
            m_priorNodeMask(node->getNodeMask()),
            m_func(func)
        {
            setEditable(true);
            setCheckable(true);
            setCheckState(Qt::Checked);
        };

        /// @brief   The destructor
        virtual ~d3DisplayItem() {};

        /// @{
        /// @brief   Accessors
        const std::string& getName() const { return m_name; };
        const osg::ref_ptr<osg::Node> getNode()  { return m_node; };
        void setNode(osg::ref_ptr<osg::Node> node) { m_node = node; };
        void setPriorNodeMask(const osg::Node::NodeMask& mask) { m_priorNodeMask = mask; };
        const osg::Node::NodeMask& getPriorNodeMask() const { return m_priorNodeMask; };
        /// @}

        /// Method to run the func
        void runFunc() const { m_func(); };

      private:

        /// The name
        std::string                 m_name;

        /// The node
        osg::ref_ptr<osg::Node>     m_node;

        /// The old node mask
        osg::Node::NodeMask         m_priorNodeMask;

        /// A registered function to run on click
        std::function<void()>       m_func;
    };

    /// @brief   Internal ethod to add an object to the osg display
    /// @param   name The name to use in the model view
    /// @param   node The node to display
    /// @param   func A function to call on clicked handle
    /// @param   enableNode Should the entry be enabled (grayed out or not?)
    /// @param   showNode Should the node be displayed or not (checked or not?)
    /// @param   myParent The parent so we can call this recursively
    bool add(const std::string& name,
             const osg::ref_ptr<osg::Node> node,
             std::function<void()>&& func,
             const bool& enableNode,
             const bool& showNode,
             const bool& addToDisplay,
             d3DisplayItem* myParent);

    /// @brief   Create a new entry in the model view
    /// @param   name The name to use in the model view
    /// @param   node The node to display
    /// @param   func A function to call on clicked handle
    /// @param   enableNode Should the entry be enabled (grayed out or not?)
    /// @param   showNode Should the node be displayed or not (checked or not?)
    /// @param   addToDisplay Should we add this to the osg display graph
    /// @param   myParent The parent so we can call this recursively
    bool createNewEntry(const std::string& name,
                        const osg::ref_ptr<osg::Node> node,
                        std::function<void()>&& func,
                        const bool& enableNode,
                        const bool& showNode,
                        const bool& addToDisplay,
                        d3DisplayItem* myParent);

    /// @brief   Method to replace a node
    /// @param   entry The entry to add to
    /// @param   node The node to display
    /// @param   func A function to call on clicked handle
    /// @param   enableNode Should the entry be enabled (grayed out or not?)
    /// @param   addToDisplay Should we add this to the osg display graph
    /// @param   myParent The parent so we can call this recursively
    bool replaceNode(d3DisplayItem* entry,
                     const osg::ref_ptr<osg::Node> node,
                     std::function<void()>&& func,
                     const bool& enableNode,
                     const bool& addToDisplay,
                     d3DisplayItem* myParent);

    /// @brief   Place to add the parent
    /// @param   parentName The name of the parent to add
    /// @param   childName The *rest* of the name to pass back for recursion
    /// @param   node The final child node that will get added (recursion)
    /// @param   func A function to call on clicked handle
    /// @param   enableNode The enableNode flag (recursion)
    /// @param   showNode The showNode flag (recursion)
    /// @parem   myParent The parent node to add this parent to
    bool addParent(const std::string& parentName,
                   const std::string& childName,
                   const osg::ref_ptr<osg::Node> node,
                   std::function<void()>&& func,
                   const bool& enableNode,
                   const bool& showNode,
                   const bool& addToDisplay,
                   d3DisplayItem* myParent);

    /// @brief   Method to find and return a pointer to the child of parent with name
    static d3DisplayItem* findChild(const d3DisplayItem* myParent,
                                    const std::string& name);

    /// @brief
    static void updateChildren(d3DisplayItem* item,
                               const bool& checked);

    /// The osg widget
    QOSGWidget*               m_pOsgWidget;

    /// The tree model
    QStandardItemModel*       m_pModel;

    /// The model protection
    std::recursive_mutex      m_mutex;
};

} // namespace d3
