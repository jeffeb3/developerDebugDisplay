/////////////////////////////////////////////////////////////////
/// @file      DisplayInterface.cpp
/// @author    Chris L Baker (clb) <chris@chimail.net>
/// @date      2014.05.20
///
/// @attention Copyright (C) 2014
/// @attention All rights reserved
/////////////////////////////////////////////////////////////////

#include "DisplayInterface.h"
#include "MainWindow.h"
#include "QOSGWidget.h"
#include "TreeView.h"

#include <iostream>
#include <chrono>

namespace d3
{

DisplayInterface& di()
{
    // forward to the singleton getter
    static std::shared_ptr<DisplayInterface> DI(DisplayInterface::get());
    return *(DI.get());
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
std::shared_ptr<DisplayInterface> DisplayInterface::get()
{
    // create the static instance
    static std::shared_ptr<DisplayInterface> pInstance;
    if ( not pInstance )
    {
        static std::mutex mutex;
        mutex.lock();
        if ( not pInstance )
            pInstance.reset( new DisplayInterface(), [](DisplayInterface* DI){ delete DI; } );
        mutex.unlock();
    }

    // return the static instance
    return pInstance;
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
bool DisplayInterface::add(const std::string& name,
                           const osg::ref_ptr<osg::Node> node,
                           const bool& addToDisplay /* = true */)
{
    // we have data - the display thread needs to know this before we setup the
    // main window
    m_haveData = true;

    // make sure the main window has been setup
    if ( not setupMainWindow() )
    {
        std::cerr << "BUMMER: No main window for you" << std::endl;
        m_haveData = false;
        return false;
    }

    // get the lock so we can add stuff
    std::lock_guard<std::mutex> l_lock(m_mutex);

    // add this node to the tree view
    // @note: the setupMainWindow() also sets up the tree view.
    static const bool showNode(true);
    return m_pTreeView->add(name, node, showNode, addToDisplay);
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
bool DisplayInterface::add(const osgGA::GUIEventAdapter::KeySymbol& key,
                           const std::function<bool(const osgGA::GUIEventAdapter&)>& func,
                           const std::string& description /* = "NONE" */)
{
    // we have data - the display thread needs to know this before we setup the
    // main window
    m_haveData = true;

    // make sure the main window has been setup
    if ( not setupMainWindow() )
    {
        std::cerr << "BUMMER: No main window for you" << std::endl;
        m_haveData = false;
        return false;
    }

    // get the lock so we can add stuff
    std::lock_guard<std::mutex> l_lock(m_mutex);

    return m_pOsgWidget->addKeyHandler(key, func, description);
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
bool DisplayInterface::add(const char& key,
                           const std::function<bool(const osgGA::GUIEventAdapter&)>& func,
                           const std::string& description /* = "NONE" */)
{
    return add((osgGA::GUIEventAdapter::KeySymbol)key, func, description);
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
bool DisplayInterface::add(const osgGA::GUIEventAdapter::MouseButtonMask& button,
                           const std::function<bool(const osgGA::GUIEventAdapter&)>& func,
                           const std::string& description /* = "NONE" */)
{
    m_haveData = true;

    if ( not setupMainWindow() )
    {
        std::cerr << "BUMMER: No main window for you" << std::endl;
        m_haveData = false;
        return false;
    }

    // get the lock so we can add stuff
    std::lock_guard<std::mutex> l_lock(m_mutex);

    return m_pOsgWidget->addClickHandler(button, func, description);
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
bool DisplayInterface::add(const std::function<bool(const osgGA::GUIEventAdapter&)>& func,
                           const std::string& description /* = "NONE" */)
{
    m_haveData = true;

    if ( not setupMainWindow() )
    {
        std::cerr << "BUMMER: No main window for you" << std::endl;
        m_haveData = false;
        return false;
    }

    // get the lock so we can add stuff
    std::lock_guard<std::mutex> l_lock(m_mutex);

    return m_pOsgWidget->addMotionEventHandler(func, description);
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
bool DisplayInterface::track(const osg::ref_ptr<osg::Node>& node,
                             const osg::Vec3d eye /* = osg::Vec3d{20.0, 20.0, 40.0} */,
                             const osg::Vec3d center /* = osg::Vec3d{0.0, 0.0, 0.0} */,
                             const osg::Vec3d up /* = osg::Vec3d{0.0, 0.0, 1.0}*/ )
{
    // we have data - the display thread needs to know this before we setup the
    // main window
    m_haveData = true;

    // make sure the main window has been setup
    if ( not setupMainWindow() )
    {
        std::cerr << "BUMMER: No main window for you" << std::endl;
        m_haveData = false;
        return false;
    }

    m_pOsgWidget->trackNode(node, eye, center, up);
    return true;
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void DisplayInterface::blockForClose()
{
    // notify that we are waiting for the drawing to close
    if ( m_haveData && m_pMainWindow && m_pMainWindow->isVisible() )
    {
        std::cout << "Waiting for window to close" << std::endl;

        // while stuff is still running
        while ( m_haveData && m_pMainWindow && m_pMainWindow->isVisible() )
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
bool DisplayInterface::running() const
{
    return (m_pMainWindow && m_pMainWindow->isVisible());
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
bool DisplayInterface::lock()
{
    // make sure the main window has been setup
    if ( not setupMainWindow() )
    {
        std::cerr << "BUMMER: No main window for you" << std::endl;
        m_haveData = false;
        return false;
    }

    m_pOsgWidget->lock();
    return true;
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
bool DisplayInterface::try_lock()
{
    if ( m_pOsgWidget )
        return m_pOsgWidget->try_lock();
    return false;
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
bool DisplayInterface::unlock()
{
    if ( m_pOsgWidget )
    {
        m_pOsgWidget->unlock();
        return true;
    }
    return false;
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
osg::ref_ptr<osg::Group> DisplayInterface::getRootGroup() const
{
    if ( m_pOsgWidget )
        return m_pOsgWidget->getRootGroup();

    return nullptr;
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
bool DisplayInterface::setRootGroup(osg::ref_ptr<osg::Group> rootGroup)
{
    if ( m_pOsgWidget )
    {
        m_pOsgWidget->setRootGroup(rootGroup);
        return true;
    }

    return false;
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void DisplayInterface::pause()
{
    std::unique_lock<std::mutex> lck(m_pauseMutex);
    std::cout << "Pausing" << std::endl;
    m_pauseNotifier.wait(lck);
    std::cout << "...done pausing" << std::endl;
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void DisplayInterface::unpause()
{
    std::cout << "calling to unpause" << std::endl;
    m_pauseNotifier.notify_all();
};

/////////////////////////////////////////////////////////////////
///////////////////// PRIVATES /////////////////////////////////
///////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
DisplayInterface::DisplayInterface() :
    m_pMainWindow(nullptr),
    m_pTreeView(nullptr),
    m_pOsgWidget(nullptr),
    m_mutex(),
    m_addNotify(),
    m_haveData(false),
    m_setupComplete(false),
    m_displayThread(),
    m_threadShouldRun(true),
    m_pauseMutex(),
    m_pauseNotifier()
{
    m_displayThread =
        std::thread
        ([&]()
         {
             m_setupComplete = false;

             // Create the pointer to the main application
             QApplication* application(nullptr);

             {
                 // get the lock
                 std::unique_lock<std::mutex> l_lock(m_mutex);

                 // wait for data to be added
                 while ( not m_haveData )
                     m_addNotify.wait(l_lock);

                 if ( not m_threadShouldRun )
                     return;

                 // faked command line args for qt
                 int argc = 1;
                 std::string arg0 = "DisplayInterface";
                 char *argv[1];
                 argv[0] = const_cast<char *>(arg0.c_str());

                 // create the QApplication
                 application = new QApplication(argc, argv);

                 // create the new main window
                 if ( nullptr == m_pMainWindow )
                 {
                     m_pMainWindow = new MainWindow();
                     m_pMainWindow->setMinimumSize(1024, 768);
                     m_pMainWindow->setSizePolicy(QSizePolicy::MinimumExpanding,
                                                  QSizePolicy::MinimumExpanding);
                 }

                 // create the osg widget
                 if ( nullptr == m_pOsgWidget )
                 {
                     m_pOsgWidget = new QOSGWidget();
                     m_pOsgWidget->initialize();
                 }

                 // pack the osg widget into the main window
                 m_pMainWindow->setOsgWidget(m_pOsgWidget);

                 // setup the m_pTree veiw for all the objets
                 if ( nullptr == m_pTreeView )
                 {
                     m_pTreeView = new TreeView();
                     m_pTreeView->setOsgWidget(m_pOsgWidget);
                 }

                 // pack this tree view into the main window
                 m_pMainWindow->setTreeView(m_pTreeView);
                 
             }

             // set the setup complete flag
             m_setupComplete = true;

             // notify the add method that we are done setting up the main window
             m_addNotify.notify_all();

             // run the application - forever
             while ( m_threadShouldRun )
             {
                 // lock osg so qt doesn't clobber osg
                 if ( m_pOsgWidget->try_lock() )
                 {
                     // lock the main window
                     if ( m_pMainWindow->try_lock() )
                     {
                         application->processEvents();
                         m_pMainWindow->unlock();
                     }
                     m_pOsgWidget->unlock();
                 }

                 // also sleep - we don't need to run full-bore even if there are not
                 // other processes running
                 std::this_thread::sleep_for(std::chrono::milliseconds(10));
             }
         });
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
DisplayInterface::~DisplayInterface()
{
    // exit the qt appliation
    QCoreApplication::exit(0);

    // see if we have been setup and need to close the main window
    if ( nullptr != m_pMainWindow )
    {
        // scoped lock
        std::lock_guard<std::mutex> l_lock(m_mutex);

        // close the window
        if ( not m_pMainWindow->close() )
            std::cerr << "BUMMER: Could not close the window" << std::endl;
    }

    m_haveData = true;
    m_threadShouldRun = false;
    if ( not m_setupComplete )
        m_addNotify.notify_all();
    m_displayThread.join();
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
bool DisplayInterface::setupMainWindow()
{
    // setup the main window if we need to
    if ( nullptr == m_pMainWindow )
    {
        // get the lock and coordinate the setup with the display thread loop
        std::unique_lock<std::mutex> l_lock(m_mutex);
        m_addNotify.notify_all();

        // wait for the setup to finish
        while ( not m_setupComplete )
            m_addNotify.wait(l_lock);
    }

    // we have a main window
    return nullptr != m_pMainWindow;
};

} // namespace d3
