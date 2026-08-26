cmake_minimum_required(VERSION 3.9.6 FATAL_ERROR)

if(WIN32 AND NOT DEFINED CMAKE_SYSTEM_VERSION)
  set(CMAKE_SYSTEM_VERSION "10.0.26100.0" CACHE STRING "Windows SDK version")
endif()

project(TrRenderer)

#--------------------------------------------------------------------------------------------------
# Keep nvpro_core optional and external. When it is unavailable, the standalone software and
# D3D12 renderers can still be configured and built; the Vulkan renderer is omitted.
set(TR_NVPRO_CORE_ROOT "" CACHE PATH "Directory that contains the external nvpro_core checkout")
option(TR_REQUIRE_NVPRO_CORE "Fail configuration when nvpro_core is unavailable" OFF)

if(TR_NVPRO_CORE_ROOT)
  set(BASE_DIRECTORY "${TR_NVPRO_CORE_ROOT}")
elseif(NOT BASE_DIRECTORY)

  find_path(BASE_DIRECTORY
    NAMES nvpro_core/cmake/setup.cmake
    PATHS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/../..
    DOC "Directory containing nvpro_core"
    )
endif()

if(EXISTS ${BASE_DIRECTORY}/nvpro_core/cmake/setup.cmake)
  set(TR_HAS_NVPRO_CORE ON)
  include(${BASE_DIRECTORY}/nvpro_core/cmake/setup.cmake)
  include(${BASE_DIRECTORY}/nvpro_core/cmake/utilities.cmake)
else()
  set(TR_HAS_NVPRO_CORE OFF)
  if(TR_REQUIRE_NVPRO_CORE)
    message(FATAL_ERROR "nvpro_core was not found; set TR_NVPRO_CORE_ROOT to its parent directory")
  endif()
  message(STATUS "nvpro_core not found: building standalone Software and D3D12 renderers only")
  set(ARCH "x64")
  set(OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_PATH}")
endif()

set(TR_RENDERER_DIR ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    add_definitions(-DNOMINMAX)
    add_definitions(/wd26812)  # 'enum class' over 'enum'
    add_definitions(/wd26451)  # Arithmetic overflow, casting 4 byte value to 8 byte value
endif()

#--------------------------------------------------------------------------------------------------
# Package shared by projects that use nvpro_core.
if(TR_HAS_NVPRO_CORE)
  _add_package_VulkanSDK()
  _add_package_ImGUI()
  _add_nvpro_core_lib()
endif()

message(STATUS "COPY ${CMAKE_CURRENT_SOURCE_DIR}/media  to  ${OUTPUT_PATH}")
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/media DESTINATION ${OUTPUT_PATH})


#--------------------------------------------------------------------------------------------------
# Sub examples
add_subdirectory(TrSoftwareRenderer)
add_subdirectory(TrD3D12Renderer)
if(TR_HAS_NVPRO_CORE)
  add_subdirectory(TrVulkanRenderer)
endif()

#--------------------------------------------------------------------------------------------------
# Install - copying the media directory
install(DIRECTORY "media"
        CONFIGURATIONS Release
        DESTINATION "bin_${ARCH}")
install(DIRECTORY "media"
        CONFIGURATIONS Debug
        DESTINATION "bin_${ARCH}_debug")
