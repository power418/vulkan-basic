cmake_minimum_required(VERSION 3.20)
project(vulkan-triangle LANGUAGES CXX C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

add_executable(main
    src/main.cpp
    src/app.cpp
    src/args.cpp
    src/shader_compiler.cpp
    src/platform/platform.cpp
    src/platform/win32/window_win32.cpp
    src/platform/win32/menubar.cpp
    src/platform/winrt/menubar.cpp
    src/platform/winrt/window_winrt.cpp
    src/renderer/vulkan_renderer.cpp
    src/res/app.manifest
)

target_link_libraries(main PRIVATE
    Vulkan::Vulkan
    runtimeobject.lib
    comctl32.lib
    windowsapp.lib
)
target_include_directories(main PRIVATE src)
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
