cmake_minimum_required(VERSION 3.28)

project(
  BLCAD
  VERSION 0.1.0
  DESCRIPTION "Parametric CAD system for Linux"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BLCAD_BUILD_GEOMETRY "Build future OCCT geometry targets" OFF)
option(BLCAD_BUILD_GUI "Build future Qt GUI targets" OFF)
option(BLCAD_BUILD_TESTS "Build unit tests" ON)

include(CTest)

if(BLCAD_BUILD_GEOMETRY)
  find_package(OpenCASCADE CONFIG REQUIRED)
  find_package(OCCT CONFIG QUIET)
endif()

if(BLCAD_BUILD_GUI)
  find_package(Qt6 COMPONENTS Widgets OpenGLWidgets REQUIRED)
endif()

add_library(
  blcad_core STATIC
  src/core/datum_plane.cpp
  src/core/dependency_graph.cpp
  src/core/error.cpp
  src/core/feature.cpp
  src/core/invalidation_state.cpp
  src/core/part_document.cpp
  src/core/part_document_json.cpp
  src/core/parameter.cpp
  src/core/quantity.cpp
  src/core/recompute_plan.cpp
  src/core/sketch.cpp
)
add_library(BLCAD::core ALIAS blcad_core)

target_include_directories(
  blcad_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(blcad_core PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(blcad_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BLCAD_BUILD_GEOMETRY)
  add_library(
    blcad_geometry STATIC
    src/geometry/circular_cut_adapter.cpp
    src/geometry/recompute_executor.cpp
    src/geometry/rectangle_extrusion_adapter.cpp
    src/geometry/shape_cache.cpp
    src/geometry/step_exporter.cpp
  )
  add_library(BLCAD::geometry ALIAS blcad_geometry)

  target_include_directories(
    blcad_geometry
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
  )
  target_include_directories(blcad_geometry SYSTEM PRIVATE ${OpenCASCADE_INCLUDE_DIR})

  target_compile_features(blcad_geometry PUBLIC cxx_std_20)

  target_link_libraries(
    blcad_geometry
    PUBLIC
      BLCAD::core
    PRIVATE
      TKPrim
      TKBO
      TKBool
      TKTopAlgo
      TKGeomAlgo
      TKSTEP
      TKXSBase
      TKBRep
      TKGeomBase
      TKG3d
      TKG2d
      TKMath
      TKernel
  )

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(blcad_geometry PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  add_executable(blcad_export_step examples/blcad_export_step.cpp)
  target_link_libraries(blcad_export_step PRIVATE BLCAD::geometry)

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(blcad_export_step PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

if(BLCAD_BUILD_TESTS)
  find_package(Catch2 3 CONFIG REQUIRED)
  include(Catch)

  add_executable(
    blcad_core_tests
    tests/core/datum_plane_tests.cpp
    tests/core/dependency_graph_tests.cpp
    tests/core/error_result_tests.cpp
    tests/core/feature_tests.cpp
    tests/core/invalidation_state_tests.cpp
    tests/core/part_document_json_tests.cpp
    tests/core/part_document_tests.cpp
    tests/core/parameter_tests.cpp
    tests/core/quantity_tests.cpp
    tests/core/recompute_plan_tests.cpp
    tests/core/sketch_tests.cpp
  )

  target_link_libraries(blcad_core_tests PRIVATE BLCAD::core Catch2::Catch2WithMain)

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(blcad_core_tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  catch_discover_tests(blcad_core_tests)

  if(BLCAD_BUILD_GEOMETRY)
    add_executable(
      blcad_geometry_tests
      tests/geometry/circular_cut_adapter_tests.cpp
      tests/geometry/recompute_executor_tests.cpp
      tests/geometry/rectangle_extrusion_adapter_tests.cpp
      tests/geometry/reference_part_pipeline_tests.cpp
      tests/geometry/shape_cache_tests.cpp
      tests/geometry/step_exporter_tests.cpp
    )

    target_link_libraries(blcad_geometry_tests PRIVATE BLCAD::geometry Catch2::Catch2WithMain)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options(blcad_geometry_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    catch_discover_tests(blcad_geometry_tests)
  endif()
endif()

message(STATUS "BLCAD MVP-1 core skeleton configured.")
message(STATUS "Geometry/OCCT targets enabled: ${BLCAD_BUILD_GEOMETRY}")
message(STATUS "GUI/Qt targets enabled: ${BLCAD_BUILD_GUI}")
