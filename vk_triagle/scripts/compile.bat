@echo off
setlocal enabledelayedexpansion

set VK_SDK=C:\VulkanSDK\1.4.350.0
set DXC="%VK_SDK%\Bin\dxc.exe"
set SRC=%~dp0..\shaders\triangle.hlsl
set OUT=%~dp0..\shaders_spv

if not exist "%OUT%" mkdir "%OUT%"

echo Compiling vertex shader...
%DXC% -T vs_6_0 -E vs_main -spirv "%SRC%" -Fo "%OUT%\triangle.vert.spv"
if !ERRORLEVEL! neq 0 (
    echo DXC failed, trying glslc entry-point...
    "%VK_SDK%\Bin\glslc.exe" -fshader-stage=vertex -x hlsl "--entry-point=vs_main" "%SRC%" -o "%OUT%\triangle.vert.spv"
)

echo Compiling fragment shader...
%DXC% -T ps_6_0 -E ps_main -spirv "%SRC%" -Fo "%OUT%\triangle.frag.spv"
if !ERRORLEVEL! neq 0 (
    echo DXC failed, trying glslc...
    "%VK_SDK%\Bin\glslc.exe" -fshader-stage=fragment -x hlsl "--entry-point=ps_main" "%SRC%" -o "%OUT%\triangle.frag.spv"
)

echo Done.
endlocal
