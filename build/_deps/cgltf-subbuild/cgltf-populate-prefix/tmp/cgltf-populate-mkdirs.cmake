# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "E:/UGit/TrRenderer/build/_deps/cgltf-src"
  "E:/UGit/TrRenderer/build/_deps/cgltf-build"
  "E:/UGit/TrRenderer/build/_deps/cgltf-subbuild/cgltf-populate-prefix"
  "E:/UGit/TrRenderer/build/_deps/cgltf-subbuild/cgltf-populate-prefix/tmp"
  "E:/UGit/TrRenderer/build/_deps/cgltf-subbuild/cgltf-populate-prefix/src/cgltf-populate-stamp"
  "E:/UGit/TrRenderer/build/_deps/cgltf-subbuild/cgltf-populate-prefix/src"
  "E:/UGit/TrRenderer/build/_deps/cgltf-subbuild/cgltf-populate-prefix/src/cgltf-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "E:/UGit/TrRenderer/build/_deps/cgltf-subbuild/cgltf-populate-prefix/src/cgltf-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "E:/UGit/TrRenderer/build/_deps/cgltf-subbuild/cgltf-populate-prefix/src/cgltf-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
