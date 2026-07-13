cmake_minimum_required(VERSION 3.20)
project(vulkan-triangle LANGUAGES CXX C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)

# --- Setup CPM.cmake ---
set(CPM_DOWNLOAD_VERSION 0.40.2)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
    message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
    file(DOWNLOAD
         https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
         ${CPM_DOWNLOAD_LOCATION}
    )
endif()
include(${CPM_DOWNLOAD_LOCATION})

# --- Fetch ImGui via CPM ---
CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG master
    DOWNLOAD_ONLY YES
)

set(IMGUI_DIR ${imgui_SOURCE_DIR})
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

add_executable(main
    src/main.cpp
    src/app.cpp
    src/args.cpp
    src/shader_compiler.cpp
    src/geometry_controller.cpp
    src/platform/platform.cpp
    src/platform/win32/window_win32.cpp
    src/platform/win32/menubar.cpp
    src/platform/winrt/menubar.cpp
    src/platform/winrt/window_winrt.cpp
    src/renderer/vulkan_renderer.cpp
    src/renderer/vulkan_texture.cpp
    src/audio/audio_wasapi.cpp
    src/res/app.manifest
    ${IMGUI_SOURCES}
)

target_compile_definitions(main PRIVATE _CRT_SECURE_NO_WARNINGS)

target_link_libraries(main PRIVATE
    Vulkan::Vulkan
    runtimeobject.lib
    comctl32.lib
    windowsapp.lib
    ole32.lib
    avrt.lib
)
target_include_directories(main PRIVATE src utils ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_compile_definitions(main PRIVATE
    VK_USE_PLATFORM_WIN32_KHR
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

find_program(DXC dxc)
find_program(GLSLC glslc)

if(DXC)
    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/shaders_spv")

    set(VERT_SPV "${CMAKE_SOURCE_DIR}/shaders_spv/triangle.vert.spv")
    set(FRAG_SPV "${CMAKE_SOURCE_DIR}/shaders_spv/triangle.frag.spv")

    add_custom_command(
        OUTPUT ${VERT_SPV}
        COMMAND ${DXC}
            -T vs_6_0 -E vs_main -spirv
            "${CMAKE_SOURCE_DIR}/shaders/triangle.hlsl"
            -Fo ${VERT_SPV}
        DEPENDS "${CMAKE_SOURCE_DIR}/shaders/triangle.hlsl"
        COMMENT "Compiling vertex shader (HLSL -> SPIR-V via DXC)"
    )

    add_custom_command(
        OUTPUT ${FRAG_SPV}
        COMMAND ${DXC}
            -T ps_6_0 -E ps_main -spirv
            "${CMAKE_SOURCE_DIR}/shaders/triangle.hlsl"
            -Fo ${FRAG_SPV}
        DEPENDS "${CMAKE_SOURCE_DIR}/shaders/triangle.hlsl"
        COMMENT "Compiling fragment shader (HLSL -> SPIR-V via DXC)"
    )

    add_custom_target(compile_shaders DEPENDS ${VERT_SPV} ${FRAG_SPV})
    add_dependencies(main compile_shaders)
elseif(GLSLC)
    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/shaders_spv")

    set(VERT_SPV "${CMAKE_SOURCE_DIR}/shaders_spv/triangle.vert.spv")
    set(FRAG_SPV "${CMAKE_SOURCE_DIR}/shaders_spv/triangle.frag.spv")

    add_custom_command(
        OUTPUT ${VERT_SPV}
        COMMAND ${GLSLC}
            -fshader-stage=vertex -x hlsl "--entry-point=vs_main"
            "${CMAKE_SOURCE_DIR}/shaders/triangle.hlsl"
            -o ${VERT_SPV}
        DEPENDS "${CMAKE_SOURCE_DIR}/shaders/triangle.hlsl"
        COMMENT "Compiling vertex shader (HLSL -> SPIR-V via GLSLC)"
    )

    add_custom_command(
        OUTPUT ${FRAG_SPV}
        COMMAND ${GLSLC}
            -fshader-stage=fragment -x hlsl "--entry-point=ps_main"
            "${CMAKE_SOURCE_DIR}/shaders/triangle.hlsl"
            -o ${FRAG_SPV}
        DEPENDS "${CMAKE_SOURCE_DIR}/shaders/triangle.hlsl"
        COMMENT "Compiling fragment shader (HLSL -> SPIR-V via GLSLC)"
    )

    add_custom_target(compile_shaders DEPENDS ${VERT_SPV} ${FRAG_SPV})
    add_dependencies(main compile_shaders)
else()
    message(WARNING "shader compiler not found -- run compile.bat first")
endif()

# ---- copy SPIR-V to target output dir so relative paths always work -------
if(TARGET main)
    # Use generator expression so multi-config (Debug/Release) works
    add_custom_command(TARGET main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:main>/shaders_spv"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/shaders_spv/triangle.vert.spv"
            "$<TARGET_FILE_DIR:main>/shaders_spv/triangle.vert.spv"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/shaders_spv/triangle.frag.spv"
            "$<TARGET_FILE_DIR:main>/shaders_spv/triangle.frag.spv"
        COMMENT "Copying SPIR-V to output directory"
    )
endif()
