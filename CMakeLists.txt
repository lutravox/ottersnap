# ── Project & toolchain ──────────────────────────────────────────────────
cmake_minimum_required(VERSION 3.17)
project(ottersnap VERSION 0.1.0 LANGUAGES CXX)

set(QT_MIN_VERSION 6.5)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt6:
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Widgets Gui)
qt_standard_project_setup(REQUIRES ${QT_MIN_VERSION})

# ── Shader pipeline ──────────────────────────────────────────────────────
#
# GLSL shaders are auto-discovered in src/resources/shaders/, compiled to
# SPIR-V by glslc, then embedded into the binary via a generated .qrc file.
find_program(GLSLC_EXECUTABLE glslc REQUIRED)

# Output directory for compiled .spv files. Placed in the build tree so
# artifacts don't pollute the source directory.
set(SPIRV_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

# Discover shader source files.
file(GLOB ALL_SHADERS
    src/resources/shaders/*.vert
    src/resources/shaders/*.frag
    src/resources/shaders/*.comp
)

# compile_shader(NAME EXT) — compiles one GLSL shader to SPIR-V.
# Creates a per-shader custom target so incremental builds recompile only
# changed shaders.
function(compile_shader NAME EXT)
    set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/shaders/${NAME}.${EXT})
    set(SPIRV ${SPIRV_DIR}/${NAME}.${EXT}.spv)

    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SPIRV_DIR}
        COMMAND ${GLSLC_EXECUTABLE} ${SRC} -o ${SPIRV}
        DEPENDS ${SRC}
        COMMENT "Compiling shader ${NAME}.${EXT} → SPIR-V"
    )

    add_custom_target(shader_${NAME}_${EXT} ALL DEPENDS ${SPIRV})
endfunction()

# Compile every discovered shader and collect the resulting .spv paths
set(SHADER_SPIRV_FILES "")

foreach(_shader IN LISTS ALL_SHADERS)
    get_filename_component(_name ${_shader} NAME_WE)
    get_filename_component(_ext ${_shader} EXT)
    string(SUBSTRING "${_ext}" 1 -1 _ext) # strip the leading dot (".frag" → "frag")

    compile_shader(${_name} ${_ext})
    list(APPEND SHADER_SPIRV_FILES ${SPIRV_DIR}/${_name}.${_ext}.spv)
endforeach()

# Generate a .qrc manifest and run rcc to produce a single C++ source file
# that embeds all compiled shaders as Qt resources. Each shader is aliased to
# `/shaders/<name>.spv` so it can be loaded at runtime via
# `QFile::read(":/shaders/main_vert.spv")` etc.
set(SHADER_RCC_QRC ${CMAKE_CURRENT_BINARY_DIR}/shaders.qrc)
set(SHADER_RCC_SRC ${CMAKE_CURRENT_BINARY_DIR}/qrc_shaders.cpp)

file(WRITE ${SHADER_RCC_QRC} "<RCC>\n    <qresource prefix=\"/\">\n")
foreach(_shader IN LISTS ALL_SHADERS)
    get_filename_component(_name ${_shader} NAME_WE)
    get_filename_component(_ext ${_shader} EXT)
    string(SUBSTRING "${_ext}" 1 -1 _ext)
    file(APPEND ${SHADER_RCC_QRC}
        "        <file alias=\"shaders/${_name}.${_ext}.spv\">${SPIRV_DIR}/${_name}.${_ext}.spv</file>\n"
    )
endforeach()
file(APPEND ${SHADER_RCC_QRC} "    </qresource>\n</RCC>\n")

add_custom_command(
    OUTPUT ${SHADER_RCC_SRC}
    COMMAND Qt6::rcc -name shaders ${SHADER_RCC_QRC} -o ${SHADER_RCC_SRC}
    # Depends on the .qrc file and the .spv output files.
    # so rcc re-runs when a shader is recompiled.
    DEPENDS ${SHADER_RCC_QRC} ${SHADER_SPIRV_FILES}
    COMMENT "Embedding shaders as Qt resource"
)

add_custom_target(shader_rcc ALL DEPENDS ${SHADER_RCC_SRC})

# ── Main executable ──────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}
    # Entry point
    src/main.cpp

    # UI
    src/ui/mainwindow.cpp
    src/ui/imagetab.cpp
    src/ui/vkimageviewer.cpp
    src/ui/notificationbar.cpp
    src/ui/thumbnailstrip.cpp
    src/ui/thumbnailbutton.cpp
    src/ui/tabbar.cpp
    src/ui/mainmenu.cpp
    src/ui/statusbar.cpp
    src/ui/viewercontainer.cpp
    src/ui/dialogs/settingsdialog.cpp

    # Core logic
    src/core/versionstore.cpp
    src/core/imageloader.cpp
    src/core/viewstate.cpp
    src/core/sessionmanager.cpp

    # Configuration
    src/config/appsettings.cpp

    # Generated shader resource
    ${SHADER_RCC_SRC}

    # Qt resource files
    src/resources/resources.qrc
)

# Ensure shaders are compiled and the .qrc is generated before the
# executable links, since ${SHADER_RCC_SRC} is part of the source list.
add_dependencies(${PROJECT_NAME} shader_rcc)
# Allows `#include "core/..."` from any source file.
target_include_directories(${PROJECT_NAME} PRIVATE src)

# ── Link libraries ───────────────────────────────────────────────────────
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Gui)

# ── Installation ─────────────────────────────────────────────────────────
# BUNDLE is for macOS (.app); RUNTIME is for Linux/Windows executables.
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ── Tests ────────────────────────────────────────────────────────────────
#
# Disabled by default. Enable with -DBUILD_TESTS=ON.
option(BUILD_TESTS "Build test suite" OFF)
if(BUILD_TESTS)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
    enable_testing()
    add_subdirectory(tests)
endif()
