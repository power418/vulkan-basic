cmake_minimum_required(VERSION 3.20)
set(CMAKE_TOOLCHAIN_FILE "C:/Users/Administrator/scoop/apps/vcpkg/current/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
project("main" LANGUAGES CXX C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenGL REQUIRED)

find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

set(SOURCE_FILES
    src/main.cpp
    utils/geometry.cpp
)

add_executable(${PROJECT_NAME}
    ${SOURCE_FILES}
)

# Force standard -I flags instead of -imsvc so clangd can parse them easily
set_target_properties(${PROJECT_NAME} PROPERTIES NO_SYSTEM_FROM_IMPORTED ON)

target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)

target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    glfw
    imgui::imgui
    runtimeobject.lib
    comctl32.lib
    windowsapp.lib
    ole32.lib
    avrt.lib
)
target_include_directories(${PROJECT_NAME} PRIVATE src utils)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

