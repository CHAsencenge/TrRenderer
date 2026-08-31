cmake_minimum_required(VERSION 3.9.6 FATAL_ERROR)

#--------------------------------------------------------------------------------------------------
# Project setting
get_filename_component(PROJNAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
# set(PROJNAME ${PROJNAME})
project(${PROJNAME} LANGUAGES C CXX)
message(STATUS "-------------------------------")
message(STATUS "Processing Project ${PROJNAME}:")

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
# C++ target and defines
set(CMAKE_CXX_STANDARD 17)
add_executable(${PROJNAME})
set_target_properties(${PROJNAME} PROPERTIES WIN32_EXECUTABLE TRUE)
if(COMMAND _add_project_definitions)
  _add_project_definitions(${PROJNAME})
endif()


#--------------------------------------------------------------------------------------------------
# Source files for this project
#
file(GLOB SOURCE_FILES *.cpp *.hpp *.inl *.h *.c)
file(GLOB EXTRA_COMMON ${TR_RENDERER_DIR}/common/*.*)
list(APPEND COMMON_SOURCE_FILES ${EXTRA_COMMON})
if(NOT TR_HAS_NVPRO_CORE)
  list(FILTER COMMON_SOURCE_FILES EXCLUDE REGEX "[/\\\\]obj_loader\\.(cpp|h)$")
endif()
include_directories(${TR_RENDERER_DIR}/common)

# Dear ImGui is consumed directly from the repository. This keeps the D3D12
# renderer independent from nvpro_core and builds only the core plus Win32/DX12
# backends required by this executable.
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
file(GLOB HLSL_SOURCE_FILES
"${TR_RENDERER_DIR}/${PROJNAME}/shaders/DxRaster/*.hlsl"
"${TR_RENDERER_DIR}/${PROJNAME}/shaders/DxRayTracing/*.hlsl"
)
set_source_files_properties(${HLSL_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

# ------------------------------------------------------------------------------------------------
# project pre define
add_definitions(-DIS_TR_D3D_RENDERER)
add_definitions(-DUNICODE)
add_definitions(-DSLN_ROOT_DIR="${TR_RENDERER_DIR}/")
add_definitions(-DSHADER_DIR="${TR_RENDERER_DIR}/${PROJNAME}/shaders/")

#--------------------------------------------------------------------------------------------------
# Sources
target_sources(${PROJNAME} PUBLIC ${SOURCE_FILES} ${HEADER_FILES})
target_sources(${PROJNAME} PUBLIC ${COMMON_SOURCE_FILES})
target_sources(${PROJNAME} PRIVATE ${IMGUI_SOURCE_FILES})
target_sources(${PROJNAME} PUBLIC ${HLSL_SOURCE_FILES})
target_include_directories(${PROJNAME} PRIVATE
  ${TR_DXC_INCLUDE_DIR}
  "${TR_IMGUI_DIR}"
  "${TR_IMGUI_DIR}/backends")

#--------------------------------------------------------------------------------------------------
# Sub-folders in Visual Studio
#
source_group("Common"       FILES ${COMMON_SOURCE_FILES})
source_group("Sources"      FILES ${SOURCE_FILES})
source_group("Headers"      FILES ${HEADER_FILES})
source_group("Shader Sources"  FILES ${HLSL_SOURCE_FILES})
source_group("ThirdParty\\Dear ImGui" FILES ${IMGUI_SOURCE_FILES})

#--------------------------------------------------------------------------------------------------
# Linkage
#
target_link_libraries(${PROJNAME} ${PLATFORM_LIBRARIES})
target_link_libraries(${PROJNAME} TrSceneCore ${TR_DXCOMPILER_LIBRARY} d3d12 dxgi windowscodecs)

add_custom_command(TARGET ${PROJNAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${TR_DXCOMPILER_DLL}" "$<TARGET_FILE_DIR:${PROJNAME}>"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${TR_DXIL_DLL}" "$<TARGET_FILE_DIR:${PROJNAME}>"
  COMMENT "Copying DXC runtime libraries")

foreach(DEBUGLIB ${LIBRARIES_DEBUG})
  target_link_libraries(${PROJNAME} debug ${DEBUGLIB})
endforeach(DEBUGLIB)

foreach(RELEASELIB ${LIBRARIES_OPTIMIZED})
  target_link_libraries(${PROJNAME} optimized ${RELEASELIB})
endforeach(RELEASELIB)

#--------------------------------------------------------------------------------------------------
# copies binaries that need to be put next to the exe files (ZLib, etc.)
#
if(COMMAND _finalize_target)
  _finalize_target(${PROJNAME})
endif()
