#include "../support/advanced_rotor_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "../support/outline_area_oracle.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/hole_instance_geometry.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_reader.hpp"

#include <BRepAlgoAPI_Check.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <numbers>

using namespace blcad;
using namespace blcad::geometry;
namespace r = blcad::test::advanced_rotor;
namespace f = blcad::test::history_fixture;
namespace {
double mm(const PartDocument& p, const char* id) {
  return p.find_parameter(ParameterId(id))->value().millimeters();
}
std::string saved(const PartDocument& p) {
  return f::checked(serialize_part_document_to_json(p));
}
void execute(const PartDocument& p, ShapeCache& c) {
  const auto result = GeometryRecomputeExecutor{}.execute_document(p, c);
  INFO((result ? "ok" : result.error().object_id() + ": " + result.error().message()));
  REQUIRE(result);
}
double volume(const GeometryShape& shape) {
  return blcad::test::integrated_solid_volume(shape).volume_mm3;
}
const GeometryShape& rotor(const ShapeCache& c) {
  const auto* shape = c.find_body_shape(r::rotor_body());
  REQUIRE(shape);
  return *shape;
}
Point3 rotate(Point3 point, double angle) {
  return {point.x * std::cos(angle) - point.y * std::sin(angle),
          point.x * std::sin(angle) + point.y * std::cos(angle), point.z};
}
double root_removed(const PartDocument& p) {
  const double a = mm(p, "root.r0"), b = mm(p, "root.r1");
  // Integral of a linear radius squared over the straight 4 mm root edge.
  return (1 - std::numbers::pi / 4) * 4 * (a * a + a * b + b * b) / 3;
}
double source_blade_volume(const PartDocument& p) {
  // The parallel translated arcs differ by 4 mm at every x from 50..90;
  // the exact rectangular neck occupies x=40..50. Area is 4*(90-40).
  return 200 * mm(p, "blade.height") - root_removed(p);
}
double expected_rotor_volume(const PartDocument& p, bool engraved = true) {
  const double radius = mm(p, "hub.radius"), height = mm(p, "hub.height"),
               direct = mm(p, "direct.depth");
  const double count = p.find_parameter(ParameterId("blade.count"))->value().count_value();
  const double mounts = p.find_parameter(ParameterId("mount.count"))->value().count_value();
  const double half_width = 2;
  // Exact circle/rectangular-neck overlap. None of the curved outer band lies
  // inside the radius45 hub, and these 5/6 blade occurrences never overlap.
  const double overlap_area = half_width * std::sqrt(radius * radius - 4) +
                              radius * radius * std::asin(half_width / radius) -
                              40 * 2 * half_width;
  const double top = 3 + height / 2;
  const double hole_radius = mm(p, "mount.minor") / 2;
  blcad::test::OutlineAreaOracle outline;
  const double label =
      mm(p, "identifier.raise.depth") * outline.area("R", mm(p, "identifier.height")) -
      (engraved ? mm(p, "identifier.recess.depth") *
                      outline.area(std::to_string(static_cast<unsigned>(count)),
                                   mm(p, "identifier.height"))
                : 0);
  return std::numbers::pi * radius * radius * (height + direct) +
         count * (source_blade_volume(p) - top * overlap_area) -
         mounts * std::numbers::pi * hole_radius * hole_radius * (height + direct) + label;
}
void material(const GeometryShape& shape, Point3 point, bool inside) {
  INFO("point " << point.x << ',' << point.y << ',' << point.z);
  CHECK(blcad::test::classify_solid_point(shape, point) ==
        (inside ? blcad::test::SolidPointLocation::Inside
                : blcad::test::SolidPointLocation::Outside));
}
void verify(const PartDocument& p, const ShapeCache& c) {
  const auto& shape = rotor(c);
  const auto expected = expected_rotor_volume(p);
  CHECK(BRepCheck_Analyzer(*OcctShapeView::get(shape), true).IsValid());
  CHECK(BRepAlgoAPI_Check(*OcctShapeView::get(shape), false, true).IsValid());
  CHECK(volume(shape) == Catch::Approx(expected).margin(1e-3));
  const auto* source = c.find_body_shape(r::blade_body());
  REQUIRE(source);
  CHECK(BRepAlgoAPI_Check(*OcctShapeView::get(*source), false, true).IsValid());
  CHECK(volume(*source) == Catch::Approx(source_blade_volume(p)).margin(2e-4));
  const double count = p.find_parameter(ParameterId("blade.count"))->value().count_value();
  for (unsigned index = 0; index < static_cast<unsigned>(count); ++index) {
    const double angle = 2 * std::numbers::pi * index / count;
    // The exact translated-circle section, independent of the three-point constructor.
    const double radius = 449. / 7;
    const double lower = 435. / 7 - std::sqrt(radius * radius - 400);
    material(shape, rotate({70, lower + 2, 8}, angle), true);
    material(shape, rotate({70, lower - .5, 8}, angle), false);
    CHECK(blcad::test::distance_to_faces(shape, rotate({70, lower, 8}, angle)) < 1e-5);
    for (const double fraction : {.15, .35, .55, .75, .85}) {
      const double radius = (1 - fraction) * mm(p, "root.r0") + fraction * mm(p, "root.r1");
      for (const double theta : {.2, .6, 1., 1.4}) {
        const Point3 witness{40 + radius - radius * std::cos(theta), -2 + 4 * fraction,
                             10 - radius + radius * std::sin(theta)};
        CHECK(blcad::test::distance_to_faces(shape, rotate(witness, angle)) < 2e-4);
      }
    }
  }
  const double hub_top = 3 + mm(p, "hub.height") / 2;
  const double bottom = 3 - mm(p, "hub.height") / 2 - mm(p, "direct.depth");
  material(shape, {0, 0, bottom + .1}, true);
  material(shape, {0, 0, bottom - .1}, false);
  const auto* hole = p.find_hole_feature(FeatureId("mount.hole"));
  REQUIRE(hole);
  REQUIRE(hole->thread());
  CHECK(hole->thread()->representation() == ThreadRepresentation::Cosmetic);
  const auto thread = f::checked(resolve_thread_definition(p, *hole->thread()));
  CHECK(thread.major_diameter_mm == 2);
  CHECK(thread.minor_diameter_mm == 1.6);
  CHECK(thread.pitch_mm == .4);
  CHECK(thread.length_mm == 3);
  for (std::size_t index = 0; index < 3; ++index) {
    const auto reference = f::checked(HoleInstanceReference::create(
        FeatureId("mount.hole"), HolePlacementId("seed"), HoleReferenceRole::Axis,
        HolePatternOccurrence{FeatureId("mount.pattern"), {index, 0}}));
    const auto axis = f::checked(HoleInstanceGeometry::resolve(p, reference, c));
    const auto center = rotate({15, 0, hub_top}, 2 * std::numbers::pi * index / 3);
    CHECK(axis.frame.origin.x == Catch::Approx(center.x).margin(1e-7));
    CHECK(axis.frame.origin.y == Catch::Approx(center.y).margin(1e-7));
    CHECK(axis.frame.origin.z == Catch::Approx(center.z).margin(1e-7));
    CHECK(axis.radius_mm == Catch::Approx(.8).margin(1e-9));
    material(shape, {center.x, center.y, hub_top - 1}, false);
    material(shape, {center.x, center.y, bottom + .1}, false);
    material(shape, {center.x + 1.1, center.y, hub_top - 1}, true);
  }
  const auto physical = f::checked(BodyResultInspector{}.inspect_physical_properties(p));
  (void)f::checked(physical.validate_current(p));
  const auto body = std::find_if(physical.data().bodies.begin(), physical.data().bodies.end(),
                                 [](const auto& item) { return item.body == r::rotor_body(); });
  REQUIRE(body != physical.data().bodies.end());
  CHECK(body->geometry.solid_count == 1);
  CHECK(physical.data().solid_body_count == 2);
  REQUIRE(body->mass);
  CHECK(body->mass->mass_kg == Catch::Approx(expected * 4e-6).margin(4e-9));
  // Part physical totals retain the hidden editable source blade. Visible STEP
  // export below contains only the one connected rotor, with its own mass above.
  CHECK(physical.data().volume_mm3 ==
        Catch::Approx(expected + source_blade_volume(p)).margin(1.2e-3));
  REQUIRE(physical.data().complete_mass());
  CHECK(physical.data().complete_mass()->mass_kg ==
        Catch::Approx((expected + source_blade_volume(p)) * 4e-6).margin(5e-9));
}
struct Directory {
  std::filesystem::path path;
  Directory() {
    char pattern[] = "/tmp/blcad-advanced-rotor-XXXXXX";
    const auto* value = ::mkdtemp(pattern);
    REQUIRE(value);
    path = value;
  }
  ~Directory() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};
std::string bytes(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  REQUIRE(file.good());
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
} // namespace

TEST_CASE("Actual revolved arc-blade rotor integrates variable roots direct hub edits and "
          "threaded patterned identification",
          "[geometry][advanced-rotor][parity-b188][X01]") {
  auto p = r::build();
  const auto baseline = saved(p);
  auto c = r::cache();
  execute(p, c);
  verify(p, c);
  REQUIRE(p.authored_history_intent());
  const auto original_history = *p.authored_history_intent();
  auto safe = original_history;
  const auto direct = std::find(safe.order.begin(), safe.order.end(), "hub.direct");
  const auto root = std::find(safe.order.begin(), safe.order.end(), "blade.root.round");
  REQUIRE(direct != safe.order.end());
  REQUIRE(root != safe.order.end());
  std::iter_swap(direct, root);
  const auto reordered = f::checked(p.with_history_intent(safe));
  auto reordered_cache = r::cache();
  execute(reordered, reordered_cache);
  CHECK(volume(rotor(reordered_cache)) == Catch::Approx(expected_rotor_volume(p)).margin(1e-3));
  auto invalid = original_history;
  std::iter_swap(std::find(invalid.order.begin(), invalid.order.end(), "feature.hub"),
                 std::find(invalid.order.begin(), invalid.order.end(), "hub.direct"));
  CHECK_FALSE(p.with_history_intent(invalid));
  auto suppressed = original_history;
  suppressed.suppressed = {"identifier.engrave"};
  const auto raised_only = f::checked(p.with_history_intent(suppressed));
  auto raised_cache = r::cache();
  execute(raised_only, raised_cache);
  CHECK(volume(rotor(raised_cache)) == Catch::Approx(expected_rotor_volume(p, false)).margin(1e-3));
  CHECK(saved(p) == baseline);
  const auto native = *OcctShapeView::get(rotor(c));
  auto failed = p;
  REQUIRE(failed.set_parameter_value(ParameterId("root.r1"), f::checked(Quantity::length_mm(80))));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(failed, c));
  CHECK(OcctShapeView::get(rotor(c))->IsSame(native));
  REQUIRE(c.validate_complete_geometry_result(p));
  CHECK(saved(p) == baseline);
  auto changed = p;
  REQUIRE(changed.set_parameter_value(ParameterId("blade.count"), f::checked(Quantity::count(6))));
  REQUIRE(
      changed.set_parameter_value(ParameterId("root.r1"), f::checked(Quantity::length_mm(.35))));
  REQUIRE(changed.set_parameter_value(ParameterId("direct.depth"),
                                      f::checked(Quantity::length_mm(1.5))));
  REQUIRE(
      changed.set_parameter_value(ParameterId("hub.height"), f::checked(Quantity::length_mm(8))));
  const auto plan = f::checked(RecomputePlan::from_graph_and_changed_nodes(
      changed.dependency_graph(), {"blade.count", "root.r1", "direct.depth", "hub.height"}));
  auto incremental = c;
  const auto result = GeometryRecomputeExecutor{}.execute_plan(changed, plan, incremental);
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  verify(changed, incremental);
  auto fresh = r::cache();
  execute(changed, fresh);
  verify(changed, fresh);
  CHECK(volume(rotor(fresh)) == Catch::Approx(volume(rotor(incremental))).margin(1e-5));
  CHECK(volume(rotor(c)) == Catch::Approx(expected_rotor_volume(p)).margin(1e-3));
  CHECK(saved(p) == baseline);
  Directory files;
  const auto json_path = files.path / "rotor.blcad.json";
  REQUIRE(write_part_document_json_file(changed, json_path));
  const auto reopened = f::checked(read_part_document_json_file(json_path));
  CHECK(saved(reopened) == saved(changed));
  auto reopened_cache = r::cache();
  execute(reopened, reopened_cache);
  verify(reopened, reopened_cache);
  const auto step_path = files.path / "rotor.step";
  const auto exported =
      f::checked(StepExporter{}.write_part_step(reopened, reopened_cache, step_path.string()));
  CHECK(exported.exported_body_count == 1);
  CHECK(exported.exported_solid_body_count == 1);
  const auto source = f::checked(StepImportSource::create(
      StepImportSourceId("rotor.exchange"), "Rotor exchange", "rotor.step",
      f::checked(step_import_sha256(bytes(step_path))), StepImportMode::EditableBody));
  const auto read = f::checked(StepImportReader{}.read(source, files.path));
  REQUIRE(read.body_count() == 1);
  double imported_volume = 0;
  for (const auto& product : read.products)
    for (const auto& body : product.bodies) {
      imported_volume += volume(body.shape);
      material(body.shape, {0, 0, -1.9}, true);
      material(body.shape, {15, 0, 5}, false);
    }
  CHECK(imported_volume == Catch::Approx(expected_rotor_volume(reopened)).margin(2e-3));
  CHECK(saved(p) == baseline);
}
