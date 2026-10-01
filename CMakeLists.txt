cmake_minimum_required(VERSION 3.25)
project(phlex_ntuple VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)          # phlex requires C++23
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "14.1")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "15")
      add_compile_options(-Wno-stringop-overflow)
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "16")
      add_compile_options(-Wno-array-bounds)
    endif()
  endif()
endif()

add_compile_options(-Wall -Wpedantic -Werror)

# --- dependencies -----------------------------------------------------------
find_package(Arrow CONFIG REQUIRED)

# phlex >= 0.4.1 (its config find_dependency()s Boost/TBB/fmt/spdlog/GSL).
find_package(phlex 0.4.1 REQUIRED)
include(GNUInstallDirs)

find_package(arrow_ntuple CONFIG REQUIRED)
find_package(phlex_arrow_common CONFIG REQUIRED)

# For the end-to-end graph test only (the library never links these): the
# existing HDF5 output path.
find_package(HDF5 QUIET COMPONENTS C)
find_package(arrow_hdf CONFIG QUIET)

# libstdc++ resolution for the Spack view (see arrow-hdf / phlex-arrow-common).
execute_process(
  COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=libstdc++.so
  OUTPUT_VARIABLE _stdc_real OUTPUT_STRIP_TRAILING_WHITESPACE)
file(REAL_PATH "${_stdc_real}" _stdc_real)
get_filename_component(_stdc_dir "${_stdc_real}" DIRECTORY)
set(PHLEX_NTUPLE_RUNTIME_LIBS "${_stdc_real}")
link_directories(BEFORE "${_stdc_dir}")
string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,-rpath,${_stdc_dir}")

# ---------------------------------------------------------------------------
# phlex_ntuple — provenance stamping + the fold accumulator.
#
# Depends on arrow-ntuple (the ntuple), phlex-arrow-common (TableGroup, cell
# helpers) and phlex::model (data_cell_index, handle).  It does NOT depend on
# arrow-hdf or phlex-arrow-hdf: persistence is the existing output node's job.
# ---------------------------------------------------------------------------
add_library(phlex_ntuple SHARED
  phlex_ntuple/Provenance.cpp
)
target_link_libraries(phlex_ntuple
  PUBLIC
    arrow_ntuple::arrow_ntuple
    phlex_arrow_common::phlex_arrow_common
    phlex::model
    Arrow::arrow_shared
)
target_include_directories(phlex_ntuple
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(test)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS phlex_ntuple
  EXPORT phlex_ntupleTargets
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
install(DIRECTORY phlex_ntuple/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/phlex_ntuple"
  FILES_MATCHING PATTERN "*.hpp"
)
install(EXPORT phlex_ntupleTargets
  FILE phlex_ntupleTargets.cmake
  NAMESPACE phlex_ntuple::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/phlex_ntuple"
)
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/phlex_ntupleConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/phlex_ntupleConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/phlex_ntupleConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/phlex_ntuple"
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/phlex_ntupleConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/phlex_ntupleConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/phlex_ntuple"
)
