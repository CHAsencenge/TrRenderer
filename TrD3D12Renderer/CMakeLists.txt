cmake_minimum_required(VERSION 3.9.6 FATAL_ERROR)

#--------------------------------------------------------------------------------------------------
# Project setting
get_filename_component(PROJNAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
# set(PROJNAME ${PROJNAME})
project(${PROJNAME} LANGUAGES C CXX)
message(STATUS "-------------------------------")
message(STATUS "Processing Project ${PROJNAME}:")

#--------------------------------------------------------------------------------------------------
# C++ target and defines
set(CMAKE_CXX_STANDARD 17)
add_executable(${PROJNAME})
set_target_properties(${PROJNAME} PROPERTIES WIN32_EXECUTABLE TRUE)
_add_project_definitions(${PROJNAME})


#--------------------------------------------------------------------------------------------------
# Source files for this project
#
file(GLOB SOURCE_FILES *.cpp *.hpp *.inl *.h *.c)
file(GLOB EXTRA_COMMON ${TR_RENDERER_DIR}/common/*.*)
list(APPEND COMMON_SOURCE_FILES ${EXTRA_COMMON})
include_directories(${TR_RENDERER_DIR}/common)
# include_directories($ENV{WindowsSDK_LibraryPath})
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
target_sources(${PROJNAME} PUBLIC ${PACKAGE_SOURCE_FILES})
target_sources(${PROJNAME} PUBLIC ${HLSL_SOURCE_FILES})

#--------------------------------------------------------------------------------------------------
# Sub-folders in Visual Studio
#
source_group("Common"       FILES ${COMMON_SOURCE_FILES} ${PACKAGE_SOURCE_FILES} nvpro_core)
source_group("Sources"      FILES ${SOURCE_FILES})
source_group("Headers"      FILES ${HEADER_FILES})
source_group("Shader Sources"  FILES ${HLSL_SOURCE_FILES})

#--------------------------------------------------------------------------------------------------
# Linkage
#
target_link_libraries(${PROJNAME} ${PLATFORM_LIBRARIES})
target_link_libraries(${PROJNAME} d3dcompiler d3d12 dxgi)

foreach(DEBUGLIB ${LIBRARIES_DEBUG})
  target_link_libraries(${PROJNAME} debug ${DEBUGLIB})
endforeach(DEBUGLIB)

foreach(RELEASELIB ${LIBRARIES_OPTIMIZED})
  target_link_libraries(${PROJNAME} optimized ${RELEASELIB})
endforeach(RELEASELIB)

#--------------------------------------------------------------------------------------------------
# copies binaries that need to be put next to the exe files (ZLib, etc.)
#
_finalize_target( ${PROJNAME} )