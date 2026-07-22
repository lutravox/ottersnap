# add_unit_test(NAME SRC [DEPS...]) — sets up a QTest executable.
# NAME: target suffix (test_<NAME>)
# SRC:  relative source file under src/
# DEPS: additional Qt modules beyond Qt6::Test
function(add_unit_test NAME SRC)
    add_executable(test_${NAME} test_${NAME}.cpp ${CMAKE_SOURCE_DIR}/src/${SRC})
    target_include_directories(test_${NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_${NAME} PRIVATE Qt6::Test ${ARGN})
    add_test(NAME test_${NAME} COMMAND test_${NAME})
endfunction()

# ── SessionManager ─────────────────────────────────────────────────
add_unit_test(sessionmanager   core/sessionmanager.cpp Qt6::Core Qt6::Widgets Qt6::Concurrent)
target_sources(test_sessionmanager PRIVATE 
    ${CMAKE_SOURCE_DIR}/src/ui/imagetab.cpp
    ${CMAKE_SOURCE_DIR}/src/core/imagesession.cpp
    ${CMAKE_SOURCE_DIR}/src/core/viewstate.cpp
    ${CMAKE_SOURCE_DIR}/src/core/imagecache.cpp
    ${CMAKE_SOURCE_DIR}/src/core/imagemonitor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/snapshotstore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/diskutils.cpp
    ${CMAKE_SOURCE_DIR}/src/config/appsettings.cpp
)

# ── ImageSession ──────────────────────────────────────────────────
add_unit_test(imagesession   core/imagesession.cpp Qt6::Core Qt6::Gui Qt6::Concurrent)
target_sources(test_imagesession PRIVATE 
    ${CMAKE_SOURCE_DIR}/src/core/imagemonitor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/snapshotstore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/diskutils.cpp
    ${CMAKE_SOURCE_DIR}/src/config/appsettings.cpp
    ${CMAKE_SOURCE_DIR}/src/core/imagecache.cpp
    ${CMAKE_SOURCE_DIR}/src/core/viewstate.cpp
)

# ── ImageMonitor ──────────────────────────────────────────────────
add_unit_test(imagemonitor   core/imagemonitor.cpp Qt6::Core)

# ── ImageCache ──────────────────────────────────────────────────────
add_unit_test(imagecache   core/imagecache.cpp Qt6::Core Qt6::Gui)

# ── ViewState ────────────────────────────────────────────────────────
add_unit_test(viewstate     core/viewstate.cpp   Qt6::Gui)

# ── ViewController ────────────────────────────────────────────────────
add_unit_test(viewcontroller controllers/viewcontroller.cpp Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent)
target_sources(test_viewcontroller PRIVATE 
    ${CMAKE_SOURCE_DIR}/src/core/imagesession.cpp
    ${CMAKE_SOURCE_DIR}/src/core/viewstate.cpp
    ${CMAKE_SOURCE_DIR}/src/core/imagemonitor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/snapshotstore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/diskutils.cpp
    ${CMAKE_SOURCE_DIR}/src/core/imagecache.cpp
    ${CMAKE_SOURCE_DIR}/src/config/appsettings.cpp
)

# ── SnapshotStore ─────────────────────────────────────────────────────
add_unit_test(snapshotstore  core/snapshotstore.cpp  Qt6::Core Qt6::Gui)
target_sources(test_snapshotstore PRIVATE ${CMAKE_SOURCE_DIR}/src/config/appsettings.cpp ${CMAKE_SOURCE_DIR}/src/core/diskutils.cpp ${CMAKE_SOURCE_DIR}/src/core/imagecache.cpp)

# ── DiskUtils ──────────────────────────────────────────────────────
add_unit_test(diskutils   core/diskutils.cpp Qt6::Core Qt6::Gui)


# ── MainWindow ─────────────────────────────────────────────────────
add_unit_test(mainwindow ui/mainwindow.cpp Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent)
target_sources(test_mainwindow PRIVATE 
    ${CMAKE_SOURCE_DIR}/src/ui/tabbar.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/emptystate.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/notification.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/viewerstate.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/snapshottimeline.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/vkimageviewer.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/statusbar.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/snapshottab.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/dialogs/settingsdialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/imagetab.cpp
    ${CMAKE_SOURCE_DIR}/src/controllers/effectscontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/controllers/viewcontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/core/sessionmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/imagesession.cpp
    ${CMAKE_SOURCE_DIR}/src/core/viewstate.cpp
    ${CMAKE_SOURCE_DIR}/src/core/imagecache.cpp
    ${CMAKE_SOURCE_DIR}/src/core/imagemonitor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/snapshotstore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/diskutils.cpp
    ${CMAKE_SOURCE_DIR}/src/config/appsettings.cpp
)
