#include "../support/engineering_resource_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/geometry/surface_adapter.hpp"

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
namespace resources = blcad::test::engineering_fixture;
namespace {
Catch::Approx approximately(double value) {
  return Catch::Approx(value).epsilon(1e-9).margin(1e-12);
}
template <typename T> T checked(Result<T> value) {
  INFO((value ? std::string{} : value.error().message()));
  REQUIRE(value);
  return std::move(value.value());
}
Quantity mm(double value) {
  return checked(Quantity::length_mm(value));
}
PartDocument part() {
  auto result = checked(PartDocument::create(DocumentId("physical"), "Physical"));
  REQUIRE(result.add_datum_plane(checked(DatumPlane::xy())));
  return result;
}
void parameter(PartDocument& p, std::string id, double value) {
  REQUIRE(p.add_parameter(checked(Parameter::create_length(ParameterId(id), id, mm(value)))));
}
void box(PartDocument& p, std::string id = "a", double width = 20, double height = 10,
         double depth = 8, bool circle = false) {
  parameter(p, id + ".w", width);
  parameter(p, id + ".h", height);
  parameter(p, id + ".d", depth);
  auto sketch = checked(Sketch::create(SketchId("sketch." + id), id, DatumPlaneId("datum.xy")));
  if (circle)
    REQUIRE(sketch.add_profile(
        checked(CircleProfile::create(ProfileId(id + ".profile"), ParameterId(id + ".w")))));
  else
    REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
        ProfileId(id + ".profile"), ParameterId(id + ".w"), ParameterId(id + ".h")))));
  REQUIRE(p.add_sketch(std::move(sketch)));
  REQUIRE(p.add_body(checked(Body::create(BodyId("body." + id), id, BodyKind::Solid))));
  auto feature = checked(Feature::create_additive_extrude(
      FeatureId(id), id, SketchId("sketch." + id), ParameterId(id + ".d")));
  REQUIRE(p.add_feature(
      checked(feature.with_body_result_context(checked(FeatureBodyResultContext::create(
          FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("body." + id)))))));
}
std::string source(const PartDocument& p) {
  return checked(serialize_part_document_to_json(p));
}
void tensor(const std::array<double, 9>& actual, const std::array<double, 9>& expected,
            double margin = 1e-10) {
  for (std::size_t i = 0; i < 9; ++i) {
    CAPTURE(i);
    CHECK(actual[i] == approximately(expected[i]).epsilon(1e-8).margin(margin));
  }
}
void principal_reconstructs(const PhysicalMassProperties& m) {
  std::array<double, 9> reconstructed{};
  for (std::size_t k = 0; k < 3; ++k) {
    const auto a = m.principal_directions[k];
    CHECK(length(a) == approximately(1));
    const std::array<double, 3> v{a.x, a.y, a.z};
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j)
        reconstructed[3 * i + j] += m.principal_moments_kg_mm2[k] * v[i] * v[j];
  }
  CHECK(m.principal_moments_kg_mm2[0] <= m.principal_moments_kg_mm2[1]);
  CHECK(m.principal_moments_kg_mm2[1] <= m.principal_moments_kg_mm2[2]);
  tensor(reconstructed, m.central_inertia_kg_mm2);
}
} // namespace

TEST_CASE("Raw box properties preserve central measures through large translations rotation scale "
          "and reflection",
          "[geometry][parity-b150][physical-properties]") {
  const auto shape =
      checked(RectangleExtrusionAdapter{}.make_extruded_rectangle(mm(20), mm(10), mm(8)));
  const auto original = checked(inspect_shape_physical_properties(shape));
  REQUIRE(original.volume);
  CHECK(original.area_mm2 == approximately(880));
  CHECK(original.volume->volume_mm3 == approximately(1600));
  CHECK(original.volume->centroid_mm.x == approximately(0).margin(1e-9));
  CHECK(original.volume->centroid_mm.y == approximately(0).margin(1e-9));
  CHECK(original.volume->centroid_mm.z == approximately(4));
  tensor(original.volume->central_inertia_mm5,
         {1600. * 164 / 12, 0, 0, 0, 1600. * 464 / 12, 0, 0, 0, 1600. * 500 / 12});
  const auto translated = checked(inspect_shape_physical_properties(
      checked(BodyTransformAdapter{}.translate(shape, {1e8, -2e8, 3e8}))));
  REQUIRE(translated.volume);
  CHECK(translated.volume->volume_mm3 == approximately(1600));
  CHECK(translated.volume->centroid_mm.x - 1e8 == approximately(0).margin(1e-6));
  CHECK(translated.volume->centroid_mm.y + 2e8 == approximately(0).margin(1e-6));
  CHECK(translated.volume->centroid_mm.z - (3e8 + 4) == approximately(0).margin(1e-6));
  tensor(translated.volume->central_inertia_mm5, original.volume->central_inertia_mm5, 1e-4);
  const auto rotated = checked(inspect_shape_physical_properties(
      checked(BodyTransformAdapter{}.rotate(shape, {}, {0, 0, 1}, 90))));
  REQUIRE(rotated.volume);
  auto expected = original.volume->central_inertia_mm5;
  std::swap(expected[0], expected[4]);
  tensor(rotated.volume->central_inertia_mm5, expected);
  const auto scaled = checked(inspect_shape_physical_properties(
      checked(BodyTransformAdapter{}.uniform_scale(shape, {}, 2))));
  REQUIRE(scaled.volume);
  CHECK(scaled.area_mm2 == approximately(original.area_mm2 * 4));
  CHECK(scaled.volume->volume_mm3 == approximately(1600 * 8));
  for (auto& value : expected)
    value = 0;
  for (std::size_t i = 0; i < 9; ++i)
    expected[i] = original.volume->central_inertia_mm5[i] * 32;
  tensor(scaled.volume->central_inertia_mm5, expected);
  const auto mirrored = checked(inspect_shape_physical_properties(
      checked(BodyTransformAdapter{}.mirror(shape, {}, {1, 0, 0}))));
  REQUIRE(mirrored.volume);
  CHECK(mirrored.volume->volume_mm3 == approximately(1600));
  tensor(mirrored.volume->central_inertia_mm5, original.volume->central_inertia_mm5);
  CHECK_FALSE(inspect_shape_physical_properties(GeometryShape{}));
}

TEST_CASE("Part box mass and cylinder principal properties match independent analytic oracles",
          "[geometry][parity-b150][physical-properties]") {
  auto p = part();
  box(p);
  resources::annotate(p);
  const auto before = source(p);
  const auto result = checked(BodyResultInspector{}.inspect_physical_properties(p));
  const auto& data = result.data();
  REQUIRE(data.complete_mass());
  CHECK(data.mass_status == PhysicalMassStatus::Complete);
  CHECK(data.complete_mass()->mass_kg == approximately(.0064));
  tensor(data.complete_mass()->central_inertia_kg_mm2,
         {.0064 * 164 / 12, 0, 0, 0, .0064 * 464 / 12, 0, 0, 0, .0064 * 500 / 12});
  CHECK(data.complete_mass()->principal_axes_unique);
  principal_reconstructs(*data.complete_mass());
  REQUIRE(result.validate_current(p));
  CHECK(source(p) == before);
  auto cylinder = part();
  box(cylinder, "cylinder", 6, 10, 8, true);
  resources::annotate(cylinder);
  const auto measured = checked(BodyResultInspector{}.inspect_physical_properties(cylinder));
  const auto& d = measured.data();
  const auto pi = std::numbers::pi;
  CHECK(d.volume_mm3 == approximately(72 * pi));
  CHECK(d.area_mm2 == approximately(66 * pi));
  REQUIRE(d.complete_mass());
  const auto m = 72 * pi * 4e-6;
  CHECK(d.complete_mass()->mass_kg == approximately(m));
  tensor(d.complete_mass()->central_inertia_kg_mm2,
         {m * 91 / 12, 0, 0, 0, m * 91 / 12, 0, 0, 0, m * 4.5});
  CHECK_FALSE(d.complete_mass()->principal_axes_unique);
  principal_reconstructs(*d.complete_mass());
}

TEST_CASE(
    "Hidden independent Bodies aggregate by material weight and the full parallel-axis tensor",
    "[geometry][parity-b150][physical-properties]") {
  auto p = part();
  box(p, "a");
  box(p, "b");
  REQUIRE(p.add_body_transform(checked(BodyTransform::create_translate(
      BodyTransformId("shift"), BodyId("body.b"), BodyTransformCoordinateSpace::World, std::nullopt,
      {30, 20, 10}, false, false))));
  REQUIRE(p.set_body_visibility(BodyId("body.b"), BodyVisibility::Hidden));
  resources::annotate(p);
  REQUIRE(p.set_body_material(BodyId("body.b"), resources::key("synthetic.material", "r2")));
  const auto result = checked(BodyResultInspector{}.inspect_physical_properties(p));
  const auto& d = result.data();
  CHECK(d.bodies.size() == 2);
  CHECK(d.volume_mm3 == approximately(3200));
  CHECK(d.area_mm2 == approximately(1760));
  REQUIRE(d.volume_centroid_mm);
  CHECK(d.volume_centroid_mm->x == approximately(15));
  CHECK(d.volume_centroid_mm->y == approximately(10));
  CHECK(d.volume_centroid_mm->z == approximately(9));
  REQUIRE(d.complete_mass());
  const auto& m = *d.complete_mass();
  CHECK(m.mass_kg == approximately(.016));
  CHECK(m.centroid_mm.x == approximately(18));
  CHECK(m.centroid_mm.y == approximately(12));
  CHECK(m.centroid_mm.z == approximately(10));
  tensor(m.central_inertia_kg_mm2, {2.138666666666667, -2.304, -1.152, -2.304, 4.458666666666667,
                                    -.768, -1.152, -.768, 5.658666666666667});
  principal_reconstructs(m);
}

TEST_CASE("Missing density distinguishes a known subset from undefined and complete mass",
          "[geometry][parity-b150][physical-properties]") {
  auto p = part();
  box(p, "a");
  box(p, "b");
  resources::annotate(p);
  REQUIRE(p.set_default_material(std::nullopt));
  REQUIRE(p.set_body_material(BodyId("body.a"), resources::key()));
  const auto partial = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(partial.data().mass_status == PhysicalMassStatus::Partial);
  CHECK_FALSE(partial.data().complete_mass());
  REQUIRE(partial.data().known_mass);
  CHECK(partial.data().known_mass->mass_kg == approximately(.0064));
  CHECK(partial.data().volume_mm3 == approximately(3200));
  CHECK(partial.data().missing_density_body_count == 1);
  REQUIRE(p.set_body_material(BodyId("body.a"), std::nullopt));
  const auto unavailable = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(unavailable.data().mass_status == PhysicalMassStatus::Unavailable);
  CHECK_FALSE(unavailable.data().known_mass);
  const auto incomplete = checked(EngineeringMaterial::create(
      resources::key("incomplete"), "No density", resources::provenance()));
  REQUIRE(p.add_engineering_resource(incomplete));
  REQUIRE(p.set_default_material(incomplete.key()));
  const auto missing = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(missing.data().mass_status == PhysicalMassStatus::Unavailable);
  CHECK(missing.data().bodies.front().mass_status == BodyPhysicalMassStatus::MissingDensity);
  REQUIRE(p.set_default_material(resources::key()));
  const auto complete = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(complete.data().complete_mass()->mass_kg == approximately(.0128));
  CHECK_FALSE(partial.validate_current(p));
}

TEST_CASE("Empty and surface-only physical scopes expose area without inventing solid mass",
          "[geometry][parity-b150][physical-properties]") {
  auto p = part();
  const auto empty = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(empty.data().bodies.empty());
  CHECK(empty.data().mass_status == PhysicalMassStatus::NotApplicable);
  CHECK_FALSE(empty.data().known_mass);
  auto path = [&](const char* id, double y) {
    auto sketch = checked(
        Sketch::create(SketchId(std::string("sketch.") + id), id, DatumPlaneId("datum.xy")));
    REQUIRE(
        sketch.add_entity(checked(LineSegment::create(SketchEntityId("line"), {0, y}, {10, y}))));
    const auto segment = checked(PathSegmentReference::create_planar(
        sketch.id(), SketchEntityId("line"), PlanarPathCurveKind::Line));
    REQUIRE(p.add_sketch(std::move(sketch)));
    REQUIRE(p.add_path_curve(checked(PathCurve::create(
        PathCurveId(id), id, {segment}, PathClosure::Open, PathOrientationRule::MinimumTwist))));
    return checked(BoundaryCurveReference::create_path_curve(PathCurveId(id)));
  };
  const auto first = path("one", 0), second = path("two", 5);
  REQUIRE(p.add_body(checked(Body::create(BodyId("surface"), "Surface", BodyKind::Surface))));
  REQUIRE(p.add_surface_feature(checked(BoundarySurfaceFeature::create(
      FeatureId("surface"), "Surface", {first, second}, BodyId("surface")))));
  resources::annotate(p);
  const auto surface = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(surface.data().surface_body_count == 1);
  CHECK(surface.data().area_mm2 == approximately(50));
  CHECK(surface.data().volume_mm3 == 0);
  CHECK(surface.data().mass_status == PhysicalMassStatus::NotApplicable);
  CHECK_FALSE(surface.data().known_mass);
  CHECK_FALSE(surface.data().bodies.front().geometry.volume);
}

TEST_CASE("Physical result freshness follows exact intent and executable lifetimes without model "
          "mutation",
          "[geometry][parity-b150][physical-properties]") {
  auto p = part();
  box(p);
  resources::annotate(p);
  const auto before = source(p);
  const auto original = p;
  const auto result = checked(BodyResultInspector{}.inspect_physical_properties(p));
  REQUIRE(result.validate_current(p));
  CHECK(source(p) == before);
  REQUIRE(p.set_default_material(resources::key("synthetic.material", "r2")));
  CHECK_FALSE(result.validate_current(p));
  REQUIRE(result.validate_current(original));
  p = original;
  const auto feature = *p.find_feature(FeatureId("a"));
  REQUIRE(p.remove_feature(feature.id()));
  REQUIRE(p.add_feature(feature));
  CHECK(source(p) == before);
  CHECK_FALSE(result.validate_current(p));
  p = original;
  REQUIRE(p.set_parameter_value(ParameterId("a.d"), mm(12)));
  CHECK_FALSE(result.validate_current(p));
  const auto updated = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(updated.data().volume_mm3 == approximately(2400));
  const auto reopened = checked(deserialize_part_document_from_json(source(p)));
  const auto restored = checked(BodyResultInspector{}.inspect_physical_properties(reopened));
  CHECK(restored.data().complete_mass()->mass_kg ==
        approximately(updated.data().complete_mass()->mass_kg));
}

TEST_CASE("Physical participation excludes consumed tools and restores additive overlapping Bodies "
          "on suppression",
          "[geometry][parity-b150][physical-properties]") {
  auto p = part();
  box(p, "a");
  box(p, "b");
  resources::annotate(p);
  REQUIRE(p.set_body_material(BodyId("body.b"), resources::key("synthetic.material", "r2")));
  REQUIRE(p.add_body_boolean_feature(checked(BodyBooleanFeature::create(
      FeatureId("combine"), BodyBooleanOperation::Add, BodyId("body.a"), {BodyId("body.b")},
      BodyBooleanResultMode::ModifyTarget, std::nullopt, false))));
  const auto legacy = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(legacy.data().solid_body_count == 1);
  CHECK(legacy.data().volume_mm3 == approximately(1600));
  CHECK(legacy.data().complete_mass()->mass_kg == approximately(.0064));
  REQUIRE(p.apply_history_intent({{"a", "b", "combine"}, {}}));
  const auto authored = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(authored.data().solid_body_count == 1);
  REQUIRE(p.apply_history_intent({{"a", "b", "combine"}, {"combine"}}));
  const auto independent = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(independent.data().solid_body_count == 2);
  CHECK(independent.data().volume_mm3 == approximately(3200));
  CHECK(independent.data().complete_mass()->mass_kg == approximately(.016));
  REQUIRE(p.apply_history_intent({{"a", "b", "combine"}, {"b", "combine"}}));
  const auto suppressed = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(suppressed.data().solid_body_count == 1);
  CHECK(suppressed.data().complete_mass()->mass_kg == approximately(.0064));
}

TEST_CASE("Near-symmetric principal axes and unrepresentable physical ranges are explicit",
          "[geometry][parity-b150][physical-properties]") {
  auto p = part();
  SECTION("nearly repeated moments have no unique principal directions") {
    box(p, "a", 20, 20 * (1 + 1e-10), 8);
    resources::annotate(p);
    const auto result = checked(BodyResultInspector{}.inspect_physical_properties(p));
    REQUIRE(result.data().complete_mass());
    CHECK_FALSE(result.data().complete_mass()->principal_axes_unique);
    principal_reconstructs(*result.data().complete_mass());
  }
  SECTION("overflow cannot masquerade as a valid mass") {
    box(p, "a", 1e6, 1e6, 1e6);
    REQUIRE(p.add_engineering_resource(resources::material("large", 1e308)));
    REQUIRE(p.set_default_material(resources::key("synthetic.material", "large")));
    const auto before = source(p);
    CHECK_FALSE(BodyResultInspector{}.inspect_physical_properties(p));
    CHECK(source(p) == before);
  }
  SECTION("underflow cannot silently produce a zero inertia tensor") {
    box(p, "a", 1e-3, 1e-3, 1e-3);
    REQUIRE(p.add_engineering_resource(resources::material("small", 1e-300)));
    REQUIRE(p.set_default_material(resources::key("synthetic.material", "small")));
    CHECK_FALSE(BodyResultInspector{}.inspect_physical_properties(p));
  }
}

TEST_CASE("Legacy aggregate physical properties retain anonymous scope and pinned Part density",
          "[geometry][parity-b150][physical-properties]") {
  auto p = part();
  parameter(p, "width", 20);
  parameter(p, "height", 10);
  parameter(p, "depth", 8);
  auto sketch = checked(Sketch::create(SketchId("sketch"), "Sketch", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
      ProfileId("profile"), ParameterId("width"), ParameterId("height")))));
  REQUIRE(p.add_sketch(std::move(sketch)));
  REQUIRE(p.add_feature(checked(Feature::create_additive_extrude(
      FeatureId("base"), "Base", SketchId("sketch"), ParameterId("depth")))));
  resources::annotate(p);
  const auto before = source(p);
  const auto result = checked(BodyResultInspector{}.inspect_physical_properties(p));
  REQUIRE(result.data().bodies.size() == 1);
  CHECK_FALSE(result.data().bodies.front().body);
  CHECK(result.data().volume_mm3 == approximately(1600));
  CHECK(result.data().complete_mass()->mass_kg == approximately(.0064));
  CHECK(source(p) == before);
}

TEST_CASE("Physical source freshness observes real editable and Reference STEP inputs",
          "[geometry][parity-b150][physical-properties][physical-imports]") {
  struct Directory {
    std::filesystem::path root;
    Directory() {
      char path[] = "/tmp/blcad-physical-input-XXXXXX";
      const auto* created = ::mkdtemp(path);
      REQUIRE(created);
      root = created;
    }
    ~Directory() {
      std::error_code ignored;
      std::filesystem::remove_all(root, ignored);
    }
  } directory;
  auto native = part();
  box(native);
  auto cache = checked(ShapeCache::create(ShapeCacheId("physical.supplier")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(native, cache));
  const auto path = directory.root / "supplier.step";
  REQUIRE(StepExporter{}.write_part_step(native, cache, path.string()));
  std::ifstream input(path, std::ios::binary);
  const std::string bytes{std::istreambuf_iterator<char>(input), {}};
  REQUIRE(!bytes.empty());
  input.close();
  const auto digest = checked(step_import_sha256(bytes));
  const auto supplier =
      checked(StepImportSource::create(StepImportSourceId("supplier"), "Supplier", "supplier.step",
                                       digest, StepImportMode::EditableBody));
  const auto geometry = checked(StepImportReader{}.read(supplier, directory.root));
  const auto catalog = checked(
      StepImportTopology{}.create_catalog(ImportedTopologyCatalogId("catalog"), geometry, 0));
  std::vector<ImportedBodyOutput> outputs;
  for (const auto& entry : catalog.entries())
    if (entry.descriptor.kind == ImportedTopologyKind::Body)
      outputs.push_back({entry.id, BodyId("imported." + std::to_string(outputs.size())),
                         *entry.descriptor.body_kind});
  const auto definition =
      checked(ImportedPartDefinition::create(supplier.id(), catalog.id(), outputs));
  const auto feature =
      checked(ImportedBodyFeature::create(FeatureId("imported"), "Imported", definition));
  auto editable = checked(PartDocument::create_editable_import(DocumentId("editable"), "Editable",
                                                               supplier, catalog, feature));
  resources::annotate(editable);
  CHECK_FALSE(BodyResultInspector{}.inspect_physical_properties(editable));
  StepImportTopologyContext context(directory.root);
  const auto result =
      checked(BodyResultInspector{}.inspect_physical_properties(editable, &context));
  CHECK(result.data().volume_mm3 == approximately(1600));
  CHECK(result.data().complete_mass()->mass_kg == approximately(.0064));
  REQUIRE(result.validate_current(editable, &context));
  CHECK_FALSE(result.validate_current(editable));
  const auto before = source(editable);
  {
    std::ofstream changed(path, std::ios::binary | std::ios::app);
    changed << '\n';
  }
  CHECK_FALSE(result.validate_current(editable, &context));
  StepImportTopologyContext changed_context(directory.root);
  CHECK_FALSE(BodyResultInspector{}.inspect_physical_properties(editable, &changed_context));
  CHECK(source(editable) == before);
  {
    std::ofstream restored(path, std::ios::binary | std::ios::trunc);
    restored << bytes;
  }
  REQUIRE(result.validate_current(editable, &context));
  const auto reference_source = checked(StepImportSource::create(
      supplier.id(), supplier.name(), "supplier.step", digest, StepImportMode::Reference));
  const auto reference = checked(PartDocument::create_reference(
      DocumentId("reference"), "Reference", reference_source, catalog, definition));
  StepImportTopologyContext reference_context(directory.root);
  const auto read_only =
      checked(BodyResultInspector{}.inspect_physical_properties(reference, &reference_context));
  CHECK(read_only.data().volume_mm3 == approximately(1600));
  CHECK(read_only.data().mass_status == PhysicalMassStatus::Unavailable);
  CHECK_FALSE(read_only.data().complete_mass());
}
