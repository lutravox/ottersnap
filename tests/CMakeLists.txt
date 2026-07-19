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

# ── ViewState ────────────────────────────────────────────────────────
add_unit_test(viewstate     core/viewstate.cpp   Qt6::Gui)

# ── VersionStore ─────────────────────────────────────────────────────
add_unit_test(versionstore  core/versionstore.cpp  Qt6::Core Qt6::Gui)

# ── AppSettings ──────────────────────────────────────────────────────
add_unit_test(appsettings   config/appsettings.cpp Qt6::Core)

# ── ImageLoader ──────────────────────────────────────────────────────
add_unit_test(imageloader   core/imageloader.cpp Qt6::Core Qt6::Gui)
