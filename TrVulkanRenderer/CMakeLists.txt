cmake_minimum_required(VERSION 3.9.6 FATAL_ERROR)

#--------------------------------------------------------------------------------------------------
# Project setting
get_filename_component(PROJNAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
# set(PROJNAME ${PROJNAME})
project(${PROJNAME} LANGUAGES C CXX)
message(STATUS "-------------------------------")
message(STATUS "Processing Project ${PROJNAME}:")
message(STATUS "current source dir ${CMAKE_CURRENT_SOURCE_DIR}:")

#--------------------------------------------------------------------------------------------------
# C++ target and defines
set(CMAKE_CXX_STANDARD 17)
add_executable(${PROJNAME})
_add_project_definitions(${PROJNAME})


#--------------------------------------------------------------------------------------------------
# Source files for this project
#
file(GLOB SOURCE_FILES *.cpp *.hpp *.inl *.h *.c)
file(GLOB EXTRA_COMMON ${TR_RENDERER_DIR}/Common/*.*)
list(APPEND COMMON_SOURCE_FILES ${EXTRA_COMMON})
include_directories(${TR_RENDERER_DIR}/Common)

# ------------------------------------------------------------------------------------------------
# project pre define
add_definitions(-DIS_TR_VULKAN_RENDERER)
add_definitions(-DUNICODE)
add_definitions(-DSLN_ROOT_DIR="${TR_RENDERER_DIR}/")
add_definitions(-DSHADER_DIR="${TR_RENDERER_DIR}/${PROJNAME}/spv/")

#--------------------------------------------------------------------------------------------------
# GLSL to SPIR-V custom build
compile_glsl_directory(
	SRC "${CMAKE_CURRENT_SOURCE_DIR}/shaders/VkRaster"
	DST "${CMAKE_CURRENT_SOURCE_DIR}/spv/VkRaster"
	VULKAN_TARGET "vulkan1.2"
	DEPENDENCY ${VULKAN_BUILD_DEPENDENCIES}
)

compile_glsl_directory(
	SRC "${CMAKE_CURRENT_SOURCE_DIR}/shaders/VkRayTracing"
	DST "${CMAKE_CURRENT_SOURCE_DIR}/spv/VkRayTracing"
	VULKAN_TARGET "vulkan1.2"
	DEPENDENCY ${VULKAN_BUILD_DEPENDENCIES}
)
message(STATUS "RT spv dir ${CMAKE_CURRENT_SOURCE_DIR}/spv/VkRayTracing:")

#--------------------------------------------------------------------------------------------------
# Sources
target_sources(${PROJNAME} PUBLIC ${SOURCE_FILES} ${HEADER_FILES})
target_sources(${PROJNAME} PUBLIC ${COMMON_SOURCE_FILES})
target_sources(${PROJNAME} PUBLIC ${PACKAGE_SOURCE_FILES})
target_sources(${PROJNAME} PUBLIC ${GLSL_SOURCES} ${GLSL_HEADERS})

#--------------------------------------------------------------------------------------------------
# Sub-folders in Visual Studio
#
source_group("Common"       FILES ${COMMON_SOURCE_FILES} ${PACKAGE_SOURCE_FILES})
source_group("Sources"      FILES ${SOURCE_FILES})
source_group("Headers"      FILES ${HEADER_FILES})
source_group("Shader Sources"  FILES ${GLSL_SOURCES})
source_group("Shader Headers"  FILES ${GLSL_HEADERS})


#--------------------------------------------------------------------------------------------------
# Linkage
#
target_link_libraries(${PROJNAME} ${PLATFORM_LIBRARIES} nvpro_core)

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


install(FILES ${SPV_OUTPUT} CONFIGURATIONS Release DESTINATION "bin_${ARCH}/${PROJNAME}/spv")
install(FILES ${SPV_OUTPUT} CONFIGURATIONS Debug DESTINATION "bin_${ARCH}_debug/${PROJNAME}/spv")