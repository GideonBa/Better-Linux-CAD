#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

TEST_CASE("AssemblyHierarchyTransformEvaluator applies authored transforms inner to outer",
          "[geometry][assembly-hierarchy-transform]") {
  const std::vector<RigidTransform> transforms{
      RigidTransform{Vector3{1.0, 0.0, 0.0}, Vector3{}},
      RigidTransform{Vector3{0.0, 10.0, 0.0}, Vector3{0.0, 0.0, 90.0}},
      RigidTransform{Vector3{100.0, 0.0, 0.0}, Vector3{0.0, 0.0, 90.0}}};

  const AssemblyHierarchyTransformEvaluator evaluator;
  const Point3 point = evaluator.evaluate_point(transforms, Point3{1.0, 0.0, 0.0});
  CHECK(point.x == Approx(88.0).margin(1.0e-12));
  CHECK(point.y == Approx(0.0).margin(1.0e-12));
  CHECK(point.z == Approx(0.0).margin(1.0e-12));

  const Vector3 vector = evaluator.evaluate_vector(transforms, Vector3{1.0, 0.0, 0.0});
  CHECK(vector.x == Approx(-1.0).margin(1.0e-12));
  CHECK(vector.y == Approx(0.0).margin(1.0e-12));
  CHECK(vector.z == Approx(0.0).margin(1.0e-12));
}

TEST_CASE("Hierarchical transform order is not treated as commutative",
          "[geometry][assembly-hierarchy-transform]") {
  const RigidTransform inner{Vector3{10.0, 0.0, 0.0}, Vector3{}};
  const RigidTransform outer{Vector3{}, Vector3{0.0, 0.0, 90.0}};
  const AssemblyHierarchyTransformEvaluator evaluator;

  const Point3 inner_then_outer =
      evaluator.evaluate_point({inner, outer}, Point3{0.0, 0.0, 0.0});
  const Point3 outer_then_inner =
      evaluator.evaluate_point({outer, inner}, Point3{0.0, 0.0, 0.0});

  CHECK(inner_then_outer.x == Approx(0.0).margin(1.0e-12));
  CHECK(inner_then_outer.y == Approx(10.0).margin(1.0e-12));
  CHECK(outer_then_inner.x == Approx(10.0).margin(1.0e-12));
  CHECK(outer_then_inner.y == Approx(0.0).margin(1.0e-12));
}

#include "assembly_flexible_subassembly_solver_tests.inc"
