cmake_minimum_required(VERSION 3.9.6 FATAL_ERROR)

#--------------------------------------------------------------------------------------------------
# Project settings
get_filename_component(PROJNAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${PROJNAME} LANGUAGES C CXX)
message(STATUS "-------------------------------")
message(STATUS "Processing Project ${PROJNAME}:")

option(TR_USE_REVERSED_Z "Use Reversed-Z depth in the D3D12 renderer" ON)
set(TR_PREPASS_MODE "OpaqueAndMasked" CACHE STRING
  "Depth/Normal prepass mode: Disabled, OpaqueOnly, or OpaqueAndMasked")
set_property(CACHE TR_PREPASS_MODE PROPERTY STRINGS
  Disabled OpaqueOnly OpaqueAndMasked)
if(TR_PREPASS_MODE STREQUAL "Disabled")
  set(TR_PREPASS_MODE_VALUE 0)
elseif(TR_PREPASS_MODE STREQUAL "OpaqueOnly")
  set(TR_PREPASS_MODE_VALUE 1)
elseif(TR_PREPASS_MODE STREQUAL "OpaqueAndMasked")
  set(TR_PREPASS_MODE_VALUE 2)
else()
  message(FATAL_ERROR
    "TR_PREPASS_MODE must be Disabled, OpaqueOnly, or OpaqueAndMasked")
endif()
message(STATUS "Depth/Normal prepass mode: ${TR_PREPASS_MODE}")

# DXC is distributed separately from the Windows SDK on some machines. Prefer
# an explicit root, then DXC_ROOT, then derive the root from dxc.exe on PATH.
set(TR_DXC_ROOT "" CACHE PATH "DirectX Shader Compiler distribution root")
if(NOT TR_DXC_ROOT AND DEFINED ENV{DXC_ROOT})
  set(TR_DXC_ROOT "$ENV{DXC_ROOT}")
endif()

find_program(TR_DXC_EXECUTABLE
  NAMES dxc.exe dxc
  HINTS "${TR_DXC_ROOT}/bin/x64")
if(NOT TR_DXC_ROOT AND TR_DXC_EXECUTABLE)
  get_filename_component(TR_DXC_BIN_DIR "${TR_DXC_EXECUTABLE}" DIRECTORY)
  get_filename_component(TR_DXC_BIN_PARENT "${TR_DXC_BIN_DIR}" DIRECTORY)
  get_filename_component(TR_DXC_ROOT "${TR_DXC_BIN_PARENT}" DIRECTORY)
endif()

find_path(TR_DXC_INCLUDE_DIR
  NAMES dxcapi.h
  HINTS "${TR_DXC_ROOT}/inc")
find_library(TR_DXCOMPILER_LIBRARY
  NAMES dxcompiler
  HINTS "${TR_DXC_ROOT}/lib/x64")
find_file(TR_DXCOMPILER_DLL
  NAMES dxcompiler.dll
  HINTS "${TR_DXC_ROOT}/bin/x64")
find_file(TR_DXIL_DLL
  NAMES dxil.dll
  HINTS "${TR_DXC_ROOT}/bin/x64")

if(NOT TR_DXC_INCLUDE_DIR OR NOT TR_DXCOMPILER_LIBRARY OR
   NOT TR_DXCOMPILER_DLL OR NOT TR_DXIL_DLL)
  message(FATAL_ERROR
    "DXC was not found. Set TR_DXC_ROOT or DXC_ROOT to a DirectX Shader Compiler distribution.")
endif()
message(STATUS "Using DXC from ${TR_DXC_ROOT}")

#--------------------------------------------------------------------------------------------------
# Target
set(CMAKE_CXX_STANDARD 17)
add_executable(${PROJNAME})
set_target_properties(${PROJNAME} PROPERTIES
  WIN32_EXECUTABLE TRUE
  FOLDER "Applications")
if(COMMAND _add_project_definitions)
  _add_project_definitions(${PROJNAME})
endif()

#--------------------------------------------------------------------------------------------------
# Source files are explicit. Physical directories are also the ownership and
# Visual Studio filter boundaries.
set(TR_APP_FILES
  Source/App/Main.cpp
  Source/App/TrRendererBase.cpp
  Source/App/TrRendererBase.h
  Source/App/TrWindowApp.cpp
  Source/App/TrWindowApp.h)

set(TR_RENDERER_FILES
  Source/Renderer/TrDeferredRenderer.cpp
  Source/Renderer/TrDeferredRenderer.h
  Source/Renderer/TrRenderConfig.h
  Source/Renderer/TrRenderConstants.h)

set(TR_RASTER_PASS_FILES
  Source/Passes/Raster/TrCompositePass.cpp
  Source/Passes/Raster/TrCompositePass.h
  Source/Passes/Raster/TrDeferredLightingPass.cpp
  Source/Passes/Raster/TrDeferredLightingPass.h
  Source/Passes/Raster/TrDepthNormalPass.cpp
  Source/Passes/Raster/TrDepthNormalPass.h
  Source/Passes/Raster/TrForwardTransparentPass.cpp
  Source/Passes/Raster/TrForwardTransparentPass.h
  Source/Passes/Raster/TrGBufferPass.cpp
  Source/Passes/Raster/TrGBufferPass.h)

set(TR_COMPUTE_PASS_FILES
  Source/Passes/Compute/TrHzbPass.cpp
  Source/Passes/Compute/TrHzbPass.h)

set(TR_LUMEN_FILES
  Source/Lumen/TrScreenProbeIrradiancePass.cpp
  Source/Lumen/TrScreenProbeIrradiancePass.h
  Source/Lumen/TrScreenProbePass.cpp
  Source/Lumen/TrScreenProbePass.h
  Source/Lumen/TrScreenProbeRadiancePass.cpp
  Source/Lumen/TrScreenProbeRadiancePass.h
  Source/Lumen/TrScreenProbeResources.cpp
  Source/Lumen/TrScreenProbeResources.h
  Source/Lumen/TrScreenTraceHit.h
  Source/Lumen/TrScreenTracePass.cpp
  Source/Lumen/TrScreenTracePass.h)

set(TR_RESOURCE_FILES
  Source/Resources/TrBuffer.cpp
  Source/Resources/TrBuffer.h
  Source/Resources/TrConstantBuffer.cpp
  Source/Resources/TrConstantBuffer.h
  Source/Resources/TrDeferredRenderTargets.cpp
  Source/Resources/TrDeferredRenderTargets.h
  Source/Resources/TrDepthNormalView.cpp
  Source/Resources/TrDepthNormalView.h
  Source/Resources/TrDescriptorHeap.cpp
  Source/Resources/TrDescriptorHeap.h
  Source/Resources/TrHistoryTexture.cpp
  Source/Resources/TrHistoryTexture.h
  Source/Resources/TrHierarchicalDepth.cpp
  Source/Resources/TrHierarchicalDepth.h
  Source/Resources/TrMaterialResources.cpp
  Source/Resources/TrMaterialResources.h
  Source/Resources/TrMesh.cpp
  Source/Resources/TrMesh.h
  Source/Resources/TrTexture.cpp
  Source/Resources/TrTexture.h)

set(TR_BACKEND_FILES
  Source/Backend/TrComputePipeline.cpp
  Source/Backend/TrComputePipeline.h
  Source/Backend/TrGraphicsPipeline.cpp
  Source/Backend/TrGraphicsPipeline.h
  Source/Backend/TrResourceBarrier.cpp
  Source/Backend/TrResourceBarrier.h
  Source/Backend/TrShaderCompiler.cpp
  Source/Backend/TrShaderCompiler.h
  Source/Backend/TrUploadContext.cpp
  Source/Backend/TrUploadContext.h)

set(TR_SCENE_FILES
  Source/Scene/TrCornellBoxScene.cpp
  Source/Scene/TrCornellBoxScene.h
  Source/Scene/TrRuntimeScene.cpp
  Source/Scene/TrRuntimeScene.h)

set(TR_DEBUG_FILES
  Source/Debug/TrGpuDebug.cpp
  Source/Debug/TrGpuDebug.h
  Source/Debug/TrGpuDebugPanel.cpp
  Source/Debug/TrGpuDebugPanel.h)

set(TR_UTILITY_FILES
  Source/Utilities/TrImageDecoder.cpp
  Source/Utilities/TrImageDecoder.h
  Source/Utilities/TrUtil.cpp
  Source/Utilities/TrUtil.h)

# Kept visible for reference but deliberately excluded from compilation.
set(TR_LEGACY_FILES
  Source/Legacy/GameTimer.cpp
  Source/Legacy/GameTimer.h
  Source/Legacy/TrVertex.cpp
  Source/Legacy/TrVertex.h)

set(TR_SOURCE_FILES
  ${TR_APP_FILES}
  ${TR_RENDERER_FILES}
  ${TR_RASTER_PASS_FILES}
  ${TR_COMPUTE_PASS_FILES}
  ${TR_LUMEN_FILES}
  ${TR_RESOURCE_FILES}
  ${TR_BACKEND_FILES}
  ${TR_SCENE_FILES}
  ${TR_DEBUG_FILES}
  ${TR_UTILITY_FILES})

set(COMMON_SOURCE_FILES
  "${TR_RENDERER_DIR}/Common/TrLog.cpp"
  "${TR_RENDERER_DIR}/Common/TrLog.h")

set(TR_THIRD_PARTY_FILES
  ThirdParty/d3dx12.h)

# Dear ImGui is consumed directly from the repository. This keeps the D3D12
# renderer independent from nvpro_core and builds only the required backends.
set(TR_IMGUI_DIR "${TR_RENDERER_DIR}/Includes/imgui")
set(IMGUI_SOURCE_FILES
  "${TR_IMGUI_DIR}/imgui.cpp"
  "${TR_IMGUI_DIR}/imgui_draw.cpp"
  "${TR_IMGUI_DIR}/imgui_tables.cpp"
  "${TR_IMGUI_DIR}/imgui_widgets.cpp"
  "${TR_IMGUI_DIR}/backends/imgui_impl_win32.cpp"
  "${TR_IMGUI_DIR}/backends/imgui_impl_dx12.cpp")
if(NOT EXISTS "${TR_IMGUI_DIR}/imgui.h")
  message(FATAL_ERROR "Standalone Dear ImGui sources were not found at ${TR_IMGUI_DIR}")
endif()

set(HLSL_SOURCE_FILES
  shaders/Raster/composite.hlsl
  shaders/Raster/deferred_lighting.hlsl
  shaders/Raster/depth_normal.hlsl
  shaders/Raster/forward_transparent.hlsl
  shaders/Raster/gbuffer.hlsl)
set(COMPUTE_HLSL_SOURCE_FILES
  shaders/Compute/hzb.hlsl)
set(LUMEN_HLSL_SOURCE_FILES
  shaders/Lumen/screen_probe_irradiance.hlsl
  shaders/Lumen/screen_probe_radiance.hlsl
  shaders/Lumen/screen_probe.hlsl
  shaders/Lumen/screen_probe_common.header.hlsl
  shaders/Lumen/screen_trace_hit.header.hlsl
  shaders/Lumen/screen_trace.hlsl)
set(COMMON_HLSL_SOURCE_FILES
  shaders/Common/deferred_lighting_common.header.hlsl
  shaders/Common/depth.header.hlsl
  shaders/Common/material_flags.header.hlsl)
set(LEGACY_HLSL_SOURCE_FILES
  shaders/Legacy/cornell_box.hlsl
  shaders/Legacy/shaders.hlsl
  shaders/Legacy/shaders_texture.hlsl)
set_source_files_properties(
  ${HLSL_SOURCE_FILES}
  ${COMPUTE_HLSL_SOURCE_FILES}
  ${LUMEN_HLSL_SOURCE_FILES}
  ${COMMON_HLSL_SOURCE_FILES}
  ${LEGACY_HLSL_SOURCE_FILES}
  ${TR_LEGACY_FILES}
  PROPERTIES HEADER_FILE_ONLY TRUE)

#--------------------------------------------------------------------------------------------------
# Compile definitions
add_definitions(-DIS_TR_D3D_RENDERER)
add_definitions(-DUNICODE)
add_definitions(-DSLN_ROOT_DIR="${TR_RENDERER_DIR}/")
add_definitions(-DSHADER_DIR="${TR_RENDERER_DIR}/${PROJNAME}/shaders/")
if(TR_USE_REVERSED_Z)
  target_compile_definitions(${PROJNAME} PRIVATE TR_REVERSED_Z=1)
else()
  target_compile_definitions(${PROJNAME} PRIVATE TR_REVERSED_Z=0)
endif()
target_compile_definitions(${PROJNAME} PRIVATE
  TR_PREPASS_MODE=${TR_PREPASS_MODE_VALUE})

#--------------------------------------------------------------------------------------------------
# Sources and include roots
target_sources(${PROJNAME} PRIVATE
  ${TR_SOURCE_FILES}
  ${TR_LEGACY_FILES}
  ${TR_THIRD_PARTY_FILES}
  ${COMMON_SOURCE_FILES}
  ${IMGUI_SOURCE_FILES}
  ${HLSL_SOURCE_FILES}
  ${COMPUTE_HLSL_SOURCE_FILES}
  ${LUMEN_HLSL_SOURCE_FILES}
  ${COMMON_HLSL_SOURCE_FILES}
  ${LEGACY_HLSL_SOURCE_FILES})

target_include_directories(${PROJNAME} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/Source"
  "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty"
  "${TR_RENDERER_DIR}/Common"
  ${TR_DXC_INCLUDE_DIR}
  "${TR_IMGUI_DIR}"
  "${TR_IMGUI_DIR}/backends")

#--------------------------------------------------------------------------------------------------
# Visual Studio filters mirror the physical module layout. Headers and sources
# remain together under their owning component.
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" PREFIX ""
  FILES ${TR_SOURCE_FILES} ${TR_LEGACY_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/shaders" PREFIX "Shaders"
  FILES
    ${HLSL_SOURCE_FILES}
    ${COMPUTE_HLSL_SOURCE_FILES}
    ${LUMEN_HLSL_SOURCE_FILES}
    ${COMMON_HLSL_SOURCE_FILES}
    ${LEGACY_HLSL_SOURCE_FILES})
source_group("Shared\\Common" FILES ${COMMON_SOURCE_FILES})
source_group("ThirdParty" FILES ${TR_THIRD_PARTY_FILES})
source_group("ThirdParty\\Dear ImGui" FILES ${IMGUI_SOURCE_FILES})

#--------------------------------------------------------------------------------------------------
# Linkage
target_link_libraries(${PROJNAME}
  ${PLATFORM_LIBRARIES}
  TrSceneCore
  ${TR_DXCOMPILER_LIBRARY}
  d3d12
  dxgi
  windowscodecs)

add_custom_command(TARGET ${PROJNAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${TR_DXCOMPILER_DLL}" "$<TARGET_FILE_DIR:${PROJNAME}>"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${TR_DXIL_DLL}" "$<TARGET_FILE_DIR:${PROJNAME}>"
  COMMENT "Copying DXC runtime libraries")

foreach(DEBUGLIB ${LIBRARIES_DEBUG})
  target_link_libraries(${PROJNAME} debug ${DEBUGLIB})
endforeach()

foreach(RELEASELIB ${LIBRARIES_OPTIMIZED})
  target_link_libraries(${PROJNAME} optimized ${RELEASELIB})
endforeach()

if(COMMAND _finalize_target)
  _finalize_target(${PROJNAME})
endif()
