# add_unit_test(NAME SRC [DEPS...]) — sets up a QTest executable.
# NAME: target suffix (test_<NAME>)
# SRC:  relative source file under src/
# DEPS: additional Qt modules beyond Qt6::Test
function(add_unit_test NAME SRC)
    add_executable(test_${NAME} test_${NAME}.cpp ${SHADER_RCC_SRC})
    target_include_directories(test_${NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_${NAME} PRIVATE ottersnap_core Qt6::Test ${ARGN})
    add_test(NAME test_${NAME} COMMAND test_${NAME})
endfunction()

# ── AppSettings ─────────────────────────────────────────────────────
add_unit_test(appsettings config/appsettings.cpp Qt6::Core)

# ── SessionManager ─────────────────────────────────────────────────
add_unit_test(sessionmanager   core/sessionmanager.cpp Qt6::Core Qt6::Widgets Qt6::Concurrent)

# ── ImageSession ──────────────────────────────────────────────────
add_unit_test(imagesession   core/imagesession.cpp Qt6::Core Qt6::Gui Qt6::Concurrent)

# ── ImageMonitor ──────────────────────────────────────────────────
add_unit_test(imagemonitor   core/imagemonitor.cpp Qt6::Core)

# ── ThumbnailCache ──────────────────────────────────────────────────────
add_unit_test(thumbnailcache   core/thumbnailcache.cpp Qt6::Core Qt6::Gui)

# ── ViewState ────────────────────────────────────────────────────────
add_unit_test(viewstate     core/viewstate.cpp   Qt6::Gui)

# ── ViewerController ────────────────────────────────────────────────────
add_unit_test(viewercontroller controllers/viewercontroller.cpp Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent)

# ── SnapshotStore ─────────────────────────────────────────────────────
add_unit_test(snapshotstore  core/snapshotstore.cpp  Qt6::Core Qt6::Gui)

# ── DiskUtils ──────────────────────────────────────────────────────
add_unit_test(diskutils   core/diskutils.cpp Qt6::Core Qt6::Gui)

add_unit_test(deltacache   core/deltacache.cpp Qt6::Core)


# ── MainWindow ─────────────────────────────────────────────────────
add_unit_test(mainwindow ui/mainwindow.cpp Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent)
