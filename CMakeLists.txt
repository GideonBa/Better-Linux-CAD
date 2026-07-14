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
  src/core/assembly_constraint.cpp
  src/core/assembly_constraint_graph.cpp
  src/core/assembly_cross_hierarchy_constraint_graph.cpp
  src/core/assembly_cross_hierarchy_motion_graph.cpp
  src/core/assembly_document.cpp
  src/core/assembly_document_json.cpp
  src/core/assembly_exchange_graph.cpp
  src/core/assembly_hierarchy.cpp
  src/core/assembly_joint.cpp
  src/core/assembly_joint_json.cpp
  src/core/assembly_joint_graph.cpp
  src/core/assembly_leaf_occurrence.cpp
  src/core/assembly_reference_target.cpp
  src/core/assembly_subassembly_instances.cpp
  src/core/body.cpp
  src/core/body_boolean_feature.cpp
  src/core/body_transform.cpp
  src/core/composite_closed_profile.cpp
  src/core/construction_geometry.cpp
  src/core/datum_axis.cpp
  src/core/datum_plane.cpp
  src/core/draft_feature.cpp
  src/core/edge_treatment_feature.cpp
  src/core/dependency_graph.cpp
  src/core/error.cpp
  src/core/feature.cpp
  src/core/generated_topology_reference.cpp
  src/core/invalidation_state.cpp
  src/core/loft_feature.cpp
  src/core/surface_feature.cpp
  src/core/mirror_feature.cpp
  src/core/part_pattern_feature.cpp
  src/core/part_document.cpp
  src/core/part_document_json.cpp
  src/core/part_feature_input_reference.cpp
  src/core/parameter.cpp
  src/core/parameter_expression.cpp
  src/core/path_curve.cpp
  src/core/project.cpp
  src/core/project_cross_hierarchy_constraints.cpp
  src/core/project_cross_hierarchy_joints.cpp
  src/core/project_json.cpp
  src/core/quantity.cpp
  src/core/recompute_plan.cpp
  src/core/reference_generated_line.cpp
  src/core/reference_recovery.cpp
  src/core/revolve_feature.cpp
  src/core/sketch.cpp
  src/core/sketch_3d.cpp
  src/core/sketch_ownership.cpp
  src/core/sketch_arc_profile.cpp
  src/core/sketch_constraints.cpp
  src/core/sketch_diagnostics.cpp
  src/core/sketch_repair_command_labels.cpp
  src/core/sketch_repair_commands.cpp
  src/core/sketch_repair_presentation_metadata.cpp
  src/core/sketch_repair_presentation_snapshot.cpp
  src/core/sketch_repair_presentation_snapshot_query.cpp
  src/core/sketch_repair_suggestions.cpp
  src/core/sketch_repair_transactions.cpp
  src/core/sketch_repair_undo_stack.cpp
  src/core/sketch_repair_undo_stack_summary.cpp
  src/core/shell_feature.cpp
  src/core/sweep_feature.cpp
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

add_executable(blcad_inspect_project_components examples/blcad_inspect_project_components.cpp)
target_link_libraries(blcad_inspect_project_components PRIVATE BLCAD::core)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(blcad_inspect_project_components PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BLCAD_BUILD_GEOMETRY)
  add_library(
    blcad_geometry STATIC
    src/geometry/arc_closed_profile_adapter.cpp
    src/geometry/assembly_concentric_constraint_equation_builder.cpp
    src/geometry/assembly_constraint_equation_builder.cpp
    src/geometry/assembly_generic_relationship_equation_builder.cpp
    src/geometry/assembly_constraint_numeric_system.cpp
    src/geometry/assembly_constraint_target_resolver.cpp
    src/geometry/assembly_contact_analyzer.cpp
    src/geometry/assembly_cross_hierarchy_constraint_solver.cpp
    src/geometry/assembly_cross_hierarchy_freshness.cpp
    src/geometry/assembly_cross_hierarchy_joint_motion_solver.cpp
    src/geometry/assembly_cross_hierarchy_motion_numeric_system.cpp
    src/geometry/assembly_cross_hierarchy_numeric_system.cpp
    src/geometry/assembly_geometric_target.cpp
    src/geometry/assembly_hierarchy_constraint_equation_builder.cpp
    src/geometry/assembly_hierarchy_revolute_joint_equation_builder.cpp
    src/geometry/assembly_hierarchy_transform_evaluator.cpp
    src/geometry/assembly_insert_constraint_equation_builder.cpp
    src/geometry/assembly_interference_analyzer.cpp
    src/geometry/assembly_joint_target_compatibility.cpp
    src/geometry/assembly_joint_drive_validation.cpp
    src/geometry/assembly_joint_motion_solver.cpp
    src/geometry/assembly_matrix_rank.cpp
    src/geometry/assembly_numeric_solve_engine.cpp
    src/geometry/assembly_occt_rigid_transform.cpp
    src/geometry/assembly_part_shape_definitions.cpp
    src/geometry/assembly_posed_leaf_shapes.cpp
    src/geometry/assembly_revolute_joint_equation_builder.cpp
    src/geometry/assembly_revolute_joint_residual.cpp
    src/geometry/assembly_prismatic_joint_equation_builder.cpp
    src/geometry/assembly_prismatic_joint_residual.cpp
    src/geometry/assembly_hierarchy_prismatic_joint_equation_builder.cpp
    src/geometry/assembly_cylindrical_joint_equation_builder.cpp
    src/geometry/assembly_cylindrical_joint_residual.cpp
    src/geometry/assembly_hierarchy_cylindrical_joint_equation_builder.cpp
    src/geometry/assembly_planar_joint_equation_builder.cpp
    src/geometry/assembly_planar_joint_residual.cpp
    src/geometry/assembly_hierarchy_planar_joint_equation_builder.cpp
    src/geometry/assembly_spherical_joint_equation_builder.cpp
    src/geometry/assembly_hierarchy_spherical_joint_equation_builder.cpp
    src/geometry/assembly_revolute_sweep_analyzer.cpp
    src/geometry/assembly_rigid_body_solver.cpp
    src/geometry/assembly_semantic_target_freshness.cpp
    src/geometry/assembly_solve_diagnostics.cpp
    src/geometry/assembly_step_exporter.cpp
    src/geometry/assembly_structured_step_exporter.cpp
    src/geometry/assembly_target_compatibility.cpp
    src/geometry/assembly_transform_evaluator.cpp
    src/geometry/body_result_inspector.cpp
    src/geometry/body_transform_adapter.cpp
    src/geometry/body_boolean_adapter.cpp
    src/geometry/circular_cut_adapter.cpp
    src/geometry/closed_profile_adapter.cpp
    src/geometry/construction_line_resolver.cpp
    src/geometry/construction_point_resolver.cpp
    src/geometry/dimension_driven_profile_resolver.cpp
    src/geometry/fillet_adapter.cpp
    src/geometry/chamfer_adapter.cpp
    src/geometry/shell_adapter.cpp
    src/geometry/draft_adapter.cpp
    src/geometry/recompute_executor.cpp
    src/geometry/circular_pattern_adapter.cpp
    src/geometry/linear_pattern_adapter.cpp
    src/geometry/loft_adapter.cpp
    src/geometry/mirror_adapter.cpp
    src/geometry/rectangle_extrusion_adapter.cpp
    src/geometry/reference_driven_sketch_helper.cpp
    src/geometry/reference_generated_profile_resolver.cpp
    src/geometry/revolve_adapter.cpp
    src/geometry/sweep_adapter.cpp
    src/geometry/surface_adapter.cpp
    src/geometry/semantic_reference_evaluator.cpp
    src/geometry/shape_cache.cpp
    src/geometry/sketch_3d_geometry_adapter.cpp
    src/geometry/sketch_reference_projector.cpp
    src/geometry/sketch_region_finder.cpp
    src/geometry/step_exporter.cpp
    src/geometry/workplane_resolver.cpp
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
      TKOffset
      TKXDESTEP
      TKXCAF
      TKCAF
      TKLCAF
      TKCDF
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

  add_executable(blcad_export_project examples/blcad_export_project.cpp)
  target_link_libraries(blcad_export_project PRIVATE BLCAD::geometry)

  add_executable(blcad_export_posed_assembly examples/blcad_export_posed_assembly.cpp)
  target_link_libraries(blcad_export_posed_assembly PRIVATE BLCAD::geometry)

  add_executable(blcad_export_structured_assembly examples/blcad_export_structured_assembly.cpp)
  target_link_libraries(blcad_export_structured_assembly PRIVATE BLCAD::geometry)

  add_executable(blcad_move_joint examples/blcad_move_joint.cpp)
  target_link_libraries(blcad_move_joint PRIVATE BLCAD::geometry)

  add_executable(blcad_analyze_assembly examples/blcad_analyze_assembly.cpp)
  target_link_libraries(blcad_analyze_assembly PRIVATE BLCAD::geometry)

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(blcad_export_step PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(blcad_export_project PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(blcad_export_posed_assembly PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(blcad_export_structured_assembly PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(blcad_move_joint PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(blcad_analyze_assembly PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

if(BLCAD_BUILD_TESTS)
  find_package(Catch2 3 CONFIG REQUIRED)
  include(Catch)

  add_executable(
    blcad_core_tests
    tests/core/assembly_angle_constraint_tests.cpp
    tests/core/assembly_constraint_graph_tests.cpp
    tests/core/assembly_constraint_tests.cpp
    tests/core/assembly_generic_relationship_tests.cpp
    tests/core/assembly_cross_hierarchy_constraint_graph_tests.cpp
    tests/core/assembly_cross_hierarchy_constraint_tests.cpp
    tests/core/assembly_cross_hierarchy_joint_tests.cpp
    tests/core/assembly_document_tests.cpp
    tests/core/assembly_exchange_graph_tests.cpp
    tests/core/assembly_exchange_name_tests.cpp
    tests/core/assembly_hierarchy_tests.cpp
    tests/core/assembly_insert_constraint_tests.cpp
    tests/core/assembly_joint_graph_tests.cpp
    tests/core/assembly_joint_tests.cpp
    tests/core/assembly_leaf_occurrence_tests.cpp
    tests/core/assembly_reference_target_json_tests.cpp
    tests/core/assembly_reference_target_tests.cpp
    tests/core/body_tests.cpp
    tests/core/body_boolean_feature_tests.cpp
    tests/core/body_transform_tests.cpp
    tests/core/body_json_tests.cpp
    tests/core/chained_construction_relation_tests.cpp
    tests/core/circular_hole_pattern_tests.cpp
    tests/core/closed_profile_json_tests.cpp
    tests/core/component_instance_tests.cpp
    tests/core/composite_closed_profile_json_tests.cpp
    tests/core/composite_closed_profile_tests.cpp
    tests/core/construction_geometry_tests.cpp
    tests/core/construction_geometry_json_tests.cpp
    tests/core/datum_axis_json_tests.cpp
    tests/core/datum_axis_tests.cpp
    tests/core/datum_plane_tests.cpp
    tests/core/dependency_graph_tests.cpp
    tests/core/derived_workplane_tests.cpp
    tests/core/draft_feature_tests.cpp
    tests/core/edge_treatment_feature_tests.cpp
    tests/core/error_result_tests.cpp
    tests/core/feature_tests.cpp
    tests/core/extrude_extent_intent_tests.cpp
    tests/core/feature_body_operation_tests.cpp
    tests/core/feature_body_operation_json_tests.cpp
    tests/core/generated_topology_reference_tests.cpp
    tests/core/invalidation_state_tests.cpp
    tests/core/loft_feature_tests.cpp
    tests/core/surface_feature_tests.cpp
    tests/core/mirror_feature_tests.cpp
    tests/core/part_document_json_tests.cpp
    tests/core/part_document_json_workplane_tests.cpp
    tests/core/part_document_tests.cpp
    tests/core/part_body_dependency_tests.cpp
    tests/core/parameter_expression_tests.cpp
    tests/core/parameter_tests.cpp
    tests/core/path_extrude_feature_tests.cpp
    tests/core/path_curve_tests.cpp
    tests/core/project_tests.cpp
    tests/core/projected_reference_invalidation_tests.cpp
    tests/core/projected_sketch_reference_json_tests.cpp
    tests/core/projected_sketch_reference_tests.cpp
    tests/core/quantity_tests.cpp
    tests/core/shell_feature_tests.cpp
    tests/core/part_feature_input_reference_tests.cpp
    tests/core/part_pattern_feature_tests.cpp
    tests/core/recompute_plan_tests.cpp
    tests/core/reference_driven_sketch_constraint_tests.cpp
    tests/core/reference_generated_line_json_tests.cpp
    tests/core/reference_recovery_tests.cpp
    tests/core/revolve_feature_tests.cpp
    tests/core/semantic_reference_invalidation_tests.cpp
    tests/core/semantic_reference_json_tests.cpp
    tests/core/sketch_arc_profile_json_tests.cpp
    tests/core/sketch_arc_profile_tests.cpp
    tests/core/sketch_diagnostics_tests.cpp
    tests/core/sketch_dimension_tests.cpp
    tests/core/sketch_repair_command_labels_tests.cpp
    tests/core/sketch_repair_commands_tests.cpp
    tests/core/sketch_repair_presentation_metadata_tests.cpp
    tests/core/sketch_repair_presentation_snapshot_query_tests.cpp
    tests/core/sketch_repair_presentation_snapshot_tests.cpp
    tests/core/sketch_repair_suggestions_tests.cpp
    tests/core/sketch_repair_transactions_tests.cpp
    tests/core/sketch_repair_undo_stack_summary_tests.cpp
    tests/core/sketch_repair_undo_stack_tests.cpp
    tests/core/sketch_spline_profile_json_tests.cpp
    tests/core/sketch_tests.cpp
    tests/core/sketch_3d_tests.cpp
    tests/core/sketch_3d_curve_tests.cpp
    tests/core/sketch_3d_json_tests.cpp
    tests/core/subassembly_instance_tests.cpp
    tests/core/sweep_feature_tests.cpp
  )

  target_link_libraries(blcad_core_tests PRIVATE BLCAD::core Catch2::Catch2WithMain)

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(blcad_core_tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  catch_discover_tests(blcad_core_tests)

  if(BLCAD_BUILD_GEOMETRY)
    add_executable(
      blcad_geometry_tests
      tests/geometry/arc_closed_profile_pipeline_tests.cpp
      tests/geometry/assembly_angle_numeric_solver_tests.cpp
      tests/geometry/assembly_concentric_constraint_equation_builder_tests.cpp
      tests/geometry/assembly_concentric_numeric_solver_tests.cpp
      tests/geometry/assembly_constraint_equation_builder_tests.cpp
      tests/geometry/assembly_generic_relationship_equation_tests.cpp
      tests/geometry/assembly_constraint_target_resolver_tests.cpp
      tests/geometry/assembly_contact_analyzer_tests.cpp
      tests/geometry/assembly_cross_hierarchy_application_tests.cpp
      tests/geometry/assembly_cross_hierarchy_constraint_solver_tests.cpp
      tests/geometry/assembly_cross_hierarchy_diagnostics_tests.cpp
      tests/geometry/assembly_cross_hierarchy_joint_motion_tests.cpp
      tests/geometry/assembly_generated_topology_target_resolver_tests.cpp
      tests/geometry/assembly_geometric_target_taxonomy_tests.cpp
      tests/geometry/assembly_hierarchy_constraint_equation_builder_tests.cpp
      tests/geometry/assembly_hierarchy_revolute_joint_equation_builder_tests.cpp
      tests/geometry/assembly_hierarchy_transform_evaluator_tests.cpp
      tests/geometry/assembly_insert_constraint_equation_builder_tests.cpp
      tests/geometry/assembly_insert_numeric_solver_tests.cpp
      tests/geometry/assembly_interference_analyzer_tests.cpp
      tests/geometry/assembly_joint_target_compatibility_tests.cpp
      tests/geometry/assembly_joint_motion_snapshot_tests.cpp
      tests/geometry/assembly_nested_step_exporter_tests.cpp
      tests/geometry/assembly_parameter_pipeline_tests.cpp
      tests/geometry/assembly_revolute_joint_motion_tests.cpp
      tests/geometry/assembly_revolute_sweep_analyzer_tests.cpp
      tests/geometry/assembly_rigid_body_solver_tests.cpp
      tests/geometry/assembly_solve_diagnostics_tests.cpp
      tests/geometry/assembly_step_exporter_tests.cpp
      tests/geometry/assembly_structured_step_exporter_tests.cpp
      tests/geometry/assembly_suppressed_component_solver_tests.cpp
      tests/geometry/assembly_target_compatibility_tests.cpp
      tests/geometry/assembly_transform_evaluator_tests.cpp
      tests/geometry/back_workplane_pipeline_tests.cpp
      tests/geometry/body_result_inspector_tests.cpp
      tests/geometry/body_transform_pipeline_tests.cpp
      tests/geometry/body_boolean_pipeline_tests.cpp
      tests/geometry/bolt_circle_pattern_pipeline_tests.cpp
      tests/geometry/chained_construction_plane_resolver_tests.cpp
      tests/geometry/circular_cut_adapter_tests.cpp
      tests/geometry/chamfer_feature_pipeline_tests.cpp
      tests/geometry/closed_profile_pipeline_tests.cpp
      tests/geometry/composite_closed_profile_pipeline_tests.cpp
      tests/geometry/construction_plane_pipeline_tests.cpp
      tests/geometry/circular_pattern_pipeline_tests.cpp
      tests/geometry/derived_workplane_pipeline_tests.cpp
      tests/geometry/extrude_extent_pipeline_tests.cpp
      tests/geometry/dimension_driven_profile_resolver_tests.cpp
      tests/geometry/front_workplane_pipeline_tests.cpp
      tests/geometry/guided_loft_pipeline_tests.cpp
      tests/geometry/fillet_feature_pipeline_tests.cpp
      tests/geometry/left_workplane_pipeline_tests.cpp
      tests/geometry/linear_pattern_pipeline_tests.cpp
      tests/geometry/loft_feature_pipeline_tests.cpp
      tests/geometry/multi_section_loft_pipeline_tests.cpp
      tests/geometry/mirror_feature_pipeline_tests.cpp
      tests/geometry/parameter_expression_pipeline_tests.cpp
      tests/geometry/path_extrude_pipeline_tests.cpp
      tests/geometry/recompute_executor_tests.cpp
      tests/geometry/rectangle_extrusion_adapter_tests.cpp
      tests/geometry/reference_driven_sketch_helper_tests.cpp
      tests/geometry/reference_generated_profile_resolver_tests.cpp
      tests/geometry/reference_part_pipeline_tests.cpp
      tests/geometry/revolve_feature_pipeline_tests.cpp
      tests/geometry/sweep_feature_pipeline_tests.cpp
      tests/geometry/surface_boundary_fill_pipeline_tests.cpp
      tests/geometry/surface_trim_extend_pipeline_tests.cpp
      tests/geometry/surface_stitch_pipeline_tests.cpp
      tests/geometry/shell_feature_pipeline_tests.cpp
      tests/geometry/draft_feature_pipeline_tests.cpp
      tests/geometry/right_workplane_pipeline_tests.cpp
      tests/geometry/semantic_reference_evaluator_tests.cpp
      tests/geometry/shape_cache_tests.cpp
      tests/geometry/sketch_3d_geometry_adapter_tests.cpp
      tests/geometry/sketch_reference_projector_tests.cpp
      tests/geometry/sketch_region_finder_tests.cpp
      tests/geometry/spline_profile_pipeline_tests.cpp
      tests/geometry/step_exporter_tests.cpp
      tests/geometry/workplane_resolver_tests.cpp
      tests/geometry/workplane_resolver_recovery_tests.cpp
    )

    target_link_libraries(blcad_geometry_tests PRIVATE BLCAD::geometry Catch2::Catch2WithMain)
    target_include_directories(blcad_geometry_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/geometry)
    target_include_directories(blcad_geometry_tests SYSTEM PRIVATE ${OpenCASCADE_INCLUDE_DIR})

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options(blcad_geometry_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    catch_discover_tests(blcad_geometry_tests)
  endif()
endif()

message(STATUS "BLCAD MVP-1 core skeleton configured.")
message(STATUS "Geometry/OCCT targets enabled: ${BLCAD_BUILD_GEOMETRY}")
message(STATUS "GUI/Qt targets enabled: ${BLCAD_BUILD_GUI}")
