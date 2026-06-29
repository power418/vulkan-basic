cmake_minimum_required(VERSION 3.25)
project("vk_hello" LANGUAGES CXX)

find_package(Vulkan REQUIRED)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

file(GLOB SOURCE_FILES
        main.cpp
        native_window_win32.cpp
)

file(GLOB HEADER_FILES
    native_window_win32.h
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCE_FILES} ${HEADER_FILES})

#target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_WIN32_KHR UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)