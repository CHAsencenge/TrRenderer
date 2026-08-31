cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

include(FetchContent)

FetchContent_Declare(
  cgltf
  URL https://github.com/jkuhlmann/cgltf/archive/refs/tags/v1.15.tar.gz
  URL_HASH SHA256=84E352092E5CD6AAB7F66DE62DDB66BEB5E6F18D412CA9D12950D7A55BFEF25A
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_GetProperties(cgltf)
if(NOT cgltf_POPULATED)
  FetchContent_Populate(cgltf)
endif()

add_library(TrSceneCore STATIC
  TrGlbImporter.cpp
  TrGlbImporter.h
  TrScene.cpp
  TrScene.h)
target_compile_features(TrSceneCore PUBLIC cxx_std_17)
target_include_directories(TrSceneCore
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
  PRIVATE "${cgltf_SOURCE_DIR}")
if(MSVC)
  target_compile_definitions(TrSceneCore PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
source_group("Sources" FILES TrGlbImporter.cpp TrScene.cpp)
source_group("Headers" FILES TrGlbImporter.h TrScene.h)

add_executable(TrSceneImporter
  TrSceneImporterMain.cpp
  "${CMAKE_CURRENT_SOURCE_DIR}/../Common/TrLog.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../Common/TrLog.h")
target_compile_features(TrSceneImporter PRIVATE cxx_std_17)
target_include_directories(TrSceneImporter PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../Common")
target_link_libraries(TrSceneImporter PRIVATE TrSceneCore)
source_group("Common" FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/../Common/TrLog.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../Common/TrLog.h")
