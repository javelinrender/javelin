cmake_minimum_required(VERSION 3.25)

project(javelin)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(JAVELIN_ENABLE_PROFILING "Enable profiling" OFF)

find_package(fastgltf REQUIRED)
find_package(glm REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Quick QuickControls2)

if(JAVELIN_ENABLE_PROFILING)
    find_package(Tracy REQUIRED)
endif()

set(SRC_FILES
    src/integrators/unidirectional_integrator.cpp
    src/integrators/metropolis_integrator.cpp
    src/ui/application.cpp
    src/ui/render_settings_model.cpp
    src/ui/viewport.cpp
    src/aabb.cpp
    src/aabb4.cpp
    src/application.cpp
    src/bvh.cpp
    src/image.cpp
    src/main.cpp
    src/material.cpp
    src/mesh.cpp
    src/path.cpp
    src/random.cpp
    src/render_settings.cpp
    src/renderer.cpp
    src/scene.cpp
    src/thread_pool.cpp
)

qt_standard_project_setup()
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)
qt_add_executable(javelin ${SRC_FILES})
qt_add_qml_module(javelin
    URI javelin_ui
    VERSION 1.0
    QML_FILES
        src/qml/Main.qml
        src/qml/ViewportPanel.qml
        src/qml/RenderSettingsPanel.qml
        src/qml/EnumParameterEditor.qml
        src/qml/IntegerParameterEditor.qml
        src/qml/BoolParameterEditor.qml
        src/qml/FilePathParameterEditor.qml
)

# Enable SIMD (SSE/AVX) for x86/x64.
if(MSVC)
    target_compile_options(javelin PRIVATE /arch:AVX2)
else()
    target_compile_options(javelin PRIVATE -mavx2 -mfma -msse4.2)
endif()

target_compile_definitions(javelin PRIVATE
    GLM_FORCE_INTRINSICS
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_SWIZZLE
)

if(JAVELIN_ENABLE_PROFILING)
    target_link_libraries(javelin PRIVATE Tracy::TracyClient)
    target_compile_definitions(javelin PRIVATE JAVELIN_ENABLE_PROFILING)
    message(STATUS "Javelin: profiling enabled")
else()
    message(STATUS "Javelin: profiling disabled")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(javelin PRIVATE JAVELIN_OS_LINUX)
    target_sources(javelin PRIVATE src/system_linux.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(javelin PRIVATE JAVELIN_OS_WINDOWS)
    target_sources(javelin PRIVATE src/system_windows.cpp)
endif()

target_include_directories(javelin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/stb_image
)

target_link_libraries(javelin PRIVATE
    fastgltf::fastgltf
    glm::glm
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickControls2
)
