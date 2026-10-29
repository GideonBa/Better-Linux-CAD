#include "blcad/core/assembly_reference_target.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/assembly_concentric_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_contact_analyzer.hpp"
#include "blcad/geometry/assembly_generic_relationship_equation_builder.hpp"
#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_hierarchy_spherical_joint_equation_builder.hpp"
#include "blcad/geometry/assembly_interaction.hpp"
#include "blcad/geometry/assembly_interference_analyzer.hpp"
#include "blcad/geometry/assembly_spherical_joint_equation_builder.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/reference_part_geometry.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <STEPCAFControl_Controller.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Compound.hxx>
#include <UnitsMethods_LengthUnit.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Pln.hxx>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

namespace {
template <typename T> T checked(Result<T> value) {
  if (!value)
    FAIL(value.error().message());
  return std::move(value.value());
}

struct Fixture {
  std::filesystem::path root;
  Fixture() {
    char name[] = "/tmp/blcad-reference-part-XXXXXX";
    const auto* directory = ::mkdtemp(name);
    REQUIRE(directory != nullptr);
    root = directory;
  }
  ~Fixture() {
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }
  std::filesystem::path path() const {
    return root / "supplier.step";
  }
  std::string bytes() const {
    std::ifstream stream(path(), std::ios::binary);
    REQUIRE(stream.good());
    return {std::istreambuf_iterator<char>(stream), {}};
  }
  void write_bytes(const std::string& data) const {
    std::ofstream stream(path(), std::ios::binary | std::ios::trunc);
    stream << data;
    REQUIRE(stream.good());
  }
  void write(const TopoDS_Shape& shape, const char* unit = "MM") const {
    Handle(TDocStd_Document) document = new TDocStd_Document("BinXCAF");
    XCAFDoc_DocumentTool::Set(document->Main());
    XCAFDoc_DocumentTool::SetLengthUnit(document, 1, UnitsMethods_LengthUnit_Millimeter);
    auto tool = XCAFDoc_DocumentTool::ShapeTool(document->Main());
    REQUIRE(!tool->AddShape(shape, false, false).IsNull());
    STEPCAFControl_Controller::Init();
    const std::string old_unit = Interface_Static::CVal("write.step.unit");
    REQUIRE(Interface_Static::SetCVal("write.step.unit", unit));
    STEPCAFControl_Writer writer;
    const bool transferred = writer.Transfer(document, STEPControl_AsIs);
    Interface_Static::SetCVal("write.step.unit", old_unit.c_str());
    REQUIRE(transferred);
    REQUIRE(writer.Write(path().c_str()) == IFSelect_RetDone);
  }
  StepImportSource source() const {
    return checked(StepImportSource::create(StepImportSourceId("supplier"), "Supplier",
                                            "supplier.step", checked(step_import_sha256(bytes())),
                                            StepImportMode::Reference));
  }
  PartDocument reference(const char* id = "reference") const {
    auto captured = source();
    auto geometry = checked(StepImportReader{}.read(captured, root));
    REQUIRE(geometry.products.size() == 1);
    auto catalog = checked(
        StepImportTopology{}.create_catalog(ImportedTopologyCatalogId("catalog"), geometry, 0));
    std::vector<ImportedBodyOutput> outputs;
    for (const auto& entry : catalog.entries()) {
      if (entry.descriptor.kind != ImportedTopologyKind::Body)
        continue;
      REQUIRE(entry.descriptor.body_kind);
      outputs.push_back({entry.id, BodyId("body." + std::to_string(outputs.size() + 1)),
                         *entry.descriptor.body_kind});
    }
    auto definition = checked(ImportedPartDefinition::create(captured.id(), catalog.id(), outputs));
    return checked(PartDocument::create_reference(DocumentId(id), "Supplier Reference", captured,
                                                  catalog, definition));
  }
};

TopoDS_Shape box(double height = 30) {
  return BRepPrimAPI_MakeBox(10, 20, height).Shape();
}
TopoDS_Shape compound(std::vector<TopoDS_Shape> shapes) {
  BRep_Builder builder;
  TopoDS_Compound result;
  builder.MakeCompound(result);
  for (const auto& shape : shapes)
    builder.Add(result, shape);
  return result;
}
ShapeCache cache() {
  return checked(ShapeCache::create(ShapeCacheId("cache")));
}
const TopoDS_Shape* topology(const GeometryShape& shape) {
  const auto* result = OcctShapeView::get(shape);
  REQUIRE(result != nullptr);
  return result;
}
void preserved(const ShapeCache& shapes, const TopoDS_Shape* previous, double volume = 6000) {
  REQUIRE(shapes.reference_result());
  CHECK(topology(shapes.reference_result()->shape) == previous);
  CHECK(RectangleExtrusionAdapter{}.summarize(shapes.reference_result()->shape).volume_mm3 ==
        Approx(volume));
}
void accept_current(PartDocument& document, const Fixture& fixture) {
  const auto expected = document.step_import_sources().front();
  REQUIRE(
      document.refresh_step_import_source(expected, checked(step_import_sha256(fixture.bytes()))));
}
} // namespace

TEST_CASE("Reference Part recompute owns explicit body results without a feature producer",
          "[geometry][assembly-step-reference-part]") {
  Fixture fixture;
  const auto* unit = GENERATE("MM", "INCH");
  fixture.write(box(), unit);
  const auto original = fixture.bytes();
  const auto document = fixture.reference();
  auto shapes = cache();
  StepImportTopologyContext context(fixture.root);
  auto recomputed = checked(GeometryRecomputeExecutor(&context).execute_document(document, shapes));
  CHECK(recomputed.executed_feature_count == 0);
  REQUIRE(shapes.reference_result());
  CHECK(shapes.reference_result()->part_document == document.id());
  CHECK(shapes.reference_result()->definition == *document.reference_part_definition());
  REQUIRE(shapes.reference_result()->bodies.size() == 1);
  CHECK(shapes.feature_shape_count() == 0);
  CHECK(shapes.body_shape_count() == 0);
  CHECK_FALSE(shapes.has_final_shape());
  CHECK(shapes.final_feature_id().empty());
  const auto& body = shapes.reference_result()->bodies.front();
  CHECK(shapes.find_body_shape(body.output.body) == &body.shape);
  CHECK(shapes.find_body_result(body.output.body) == nullptr);
  const auto inspection = checked(BodyResultInspector{}.inspect(document, shapes));
  REQUIRE(inspection.bodies.size() == 1);
  CHECK(inspection.solid_body_count == 1);
  CHECK_FALSE(inspection.bodies.front().source_feature_id);
  REQUIRE(inspection.bodies.front().shape_summary);
  CHECK(inspection.bodies.front().shape_summary->volume_mm3 == Approx(6000));
  const auto final =
      checked(BodyResultInspector{}.resolve_compatible_final_shape(document, shapes));
  CHECK(RectangleExtrusionAdapter{}.summarize(final).volume_mm3 == Approx(6000));

  auto canonical = checked(serialize_part_document_to_json(document));
  const auto reopened = checked(deserialize_part_document_from_json(canonical));
  CHECK(checked(serialize_part_document_to_json(reopened)) == canonical);
  auto reopened_shapes = cache();
  StepImportTopologyContext reopened_context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor(&reopened_context).execute_document(reopened, reopened_shapes));
  CHECK(reopened_shapes.validated_reference_result(reopened));
  CHECK(fixture.bytes() == original);
}

TEST_CASE(
    "Reference cache rejects mixed or stale ownership and explicit clear removes its authority",
    "[geometry][assembly-step-reference-part]") {
  Fixture fixture;
  fixture.write(box());
  const auto document = fixture.reference();
  auto shapes = cache();
  StepImportTopologyContext context(fixture.root);
  REQUIRE(ReferencePartGeometry{}.seed_reference(document, context, shapes));
  const auto previous = topology(shapes.reference_result()->shape);
  const auto shape = shapes.reference_result()->shape;
  CHECK_FALSE(shapes.store_feature_shape(FeatureId("invented"), shape));
  CHECK_FALSE(shapes.set_final_shape(FeatureId("invented"), shape));
  CHECK_FALSE(shapes.store_body_shape(BodyId("body.1"), FeatureId("invented"), shape));
  CHECK_FALSE(BodyResultInspector{}.inspect(fixture.reference("other-owner"), shapes));
  CHECK_FALSE(BodyResultInspector{}.resolve_compatible_final_shape(
      checked(PartDocument::create(DocumentId("authored"), "Authored")), shapes));
  auto forged = *shapes.reference_result();
  forged.bodies.front().output.kind = BodyKind::Surface;
  CHECK_FALSE(shapes.store_reference_result(forged));
  preserved(shapes, previous);
  shapes.clear_final_shape();
  preserved(shapes, previous);
  shapes.clear();
  CHECK(shapes.reference_result() == nullptr);
  CHECK(shapes.find_body_shape(BodyId("body.1")) == nullptr);
  CHECK_FALSE(shapes.validated_reference_result(document));
  REQUIRE(shapes.store_feature_shape(FeatureId("authored"), shape));
  CHECK_FALSE(BodyResultInspector{}.inspect(document, shapes));
  REQUIRE(ReferencePartGeometry{}.seed_reference(document, context, shapes));
  CHECK(shapes.feature_shape_count() == 0);
}

TEST_CASE("Reference full definition supports multiple solids and surface bodies",
          "[geometry][assembly-step-reference-part]") {
  Fixture fixture;
  const auto surface =
      BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(100, 0, 0), gp_Dir(0, 0, 1)), 0, 7, 0, 9).Shape();
  const bool surface_only = GENERATE(false, true);
  fixture.write(
      surface_only
          ? surface
          : compound({box(), BRepPrimAPI_MakeBox(gp_Pnt(40, 0, 0), 2, 3, 4).Shape(), surface}));
  const auto original = fixture.bytes();
  const auto document = fixture.reference();
  auto shapes = cache();
  StepImportTopologyContext context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor(&context).execute_document(document, shapes));
  const auto inspection = checked(BodyResultInspector{}.inspect(document, shapes));
  CHECK(inspection.surface_body_count == 1);
  CHECK(inspection.solid_body_count == (surface_only ? 0 : 2));
  CHECK(inspection.bodies.size() == (surface_only ? 1 : 3));
  const auto definition =
      checked(BodyResultInspector{}.resolve_compatible_final_shape(document, shapes));
  const auto summary = RectangleExtrusionAdapter{}.summarize(definition);
  CHECK(summary.solid_count == (surface_only ? 0 : 2));
  // VolumeProperties over a compound containing open faces is not a physical
  // solid-volume metric. Check each actual solid and the separate surface area.
  double solid_volume = 0;
  double surface_area = 0;
  REQUIRE(shapes.reference_result());
  for (const auto& body : shapes.reference_result()->bodies) {
    const auto body_summary = RectangleExtrusionAdapter{}.summarize(body.shape);
    if (body.output.kind == BodyKind::Solid) {
      CHECK(body_summary.solid_count == 1);
      solid_volume += body_summary.volume_mm3;
    } else {
      CHECK(body_summary.solid_count == 0);
      GProp_GProps properties;
      BRepGProp::SurfaceProperties(*topology(body.shape), properties);
      surface_area += properties.Mass();
    }
  }
  CHECK(solid_volume == Approx(surface_only ? 0 : 6024).margin(1e-7));
  CHECK(surface_area == Approx(63).margin(1e-7));
  BRepAlgoAPI_Common common(*topology(definition), *topology(definition));
  REQUIRE(common.IsDone());
  TopTools_IndexedMapOfShape common_faces, common_solids, solid_faces;
  TopExp::MapShapes(common.Shape(), TopAbs_FACE, common_faces);
  TopExp::MapShapes(common.Shape(), TopAbs_SOLID, common_solids);
  for (int index = 1; index <= common_solids.Extent(); ++index)
    TopExp::MapShapes(common_solids(index), TopAbs_FACE, solid_faces);
  REQUIRE(common_faces.Extent() > solid_faces.Extent());
  if (!surface_only) {
    GProp_GProps raw_properties;
    BRepGProp::VolumeProperties(common.Shape(), raw_properties);
    INFO("Raw mixed-compound pseudo-volume: " << raw_properties.Mass());
    CHECK(std::abs(raw_properties.Mass() - 6024) > 1e-3);
  }
  auto assembly = checked(AssemblyDocument::create(DocumentId("assembly.mixed"), "Mixed"));
  REQUIRE(assembly.add_member_part(document.id()));
  for (const auto* id : {"first", "second"})
    REQUIRE(assembly.add_component_instance(checked(ComponentInstance::create(
        ComponentInstanceId(id), id, document.id(), ComponentVisibility::Visible,
        ComponentSuppressionState::Active, ComponentGroundingState::Free, {}))));
  auto project = checked(Project::create(DocumentId("project.mixed"), "Mixed", assembly));
  REQUIRE(project.add_part_document(document));
  const auto contact = checked(AssemblyContactAnalyzer{&context}.analyze(project));
  REQUIRE(contact.records.size() == 1);
  CHECK(contact.records.front().overlap_volume_mm3 == Approx(solid_volume).margin(1e-7));
  CHECK(contact.records.front().classification ==
        (surface_only ? AssemblyContactClassification::Touching
                      : AssemblyContactClassification::Interfering));
  const auto interference = checked(AssemblyInterferenceAnalyzer{&context}.analyze(project));
  CHECK(interference.interferences.size() == (surface_only ? 0 : 1));
  if (!surface_only)
    CHECK(interference.interferences.front().overlap_volume_mm3 == Approx(6024).margin(1e-7));
  const auto clearance = checked(AssemblyClearanceAnalyzer{&context}.analyze(project));
  CHECK(clearance.interferences.size() == (surface_only ? 0 : 1));
  CHECK(clearance.clearance_violations.size() == (surface_only ? 1 : 0));
  if (surface_only)
    CHECK(clearance.clearance_violations.front().minimum_distance_mm == Approx(0).margin(1e-7));
  REQUIRE(StepExporter{&context}.write_part_step(document, shapes,
                                                 (fixture.root / "export.step").string()));
  CHECK(fixture.bytes() == original);
}

TEST_CASE("Reference recompute failures preserve prior geometry for full and incremental APIs",
          "[geometry][assembly-step-reference-part]") {
  Fixture fixture;
  fixture.write(box());
  auto document = fixture.reference();
  const auto original = fixture.bytes();
  auto shapes = cache();
  StepImportTopologyContext initial(fixture.root);
  REQUIRE(GeometryRecomputeExecutor(&initial).execute_document(document, shapes));
  const auto previous = topology(shapes.reference_result()->shape);
  const auto plan = checked(document.create_recompute_plan());
  const bool incremental = GENERATE(false, true);
  const auto execute = [&](StepImportTopologyContext* context) {
    const GeometryRecomputeExecutor executor(context);
    return incremental ? executor.execute_plan(document, plan, shapes)
                       : executor.execute_document(document, shapes);
  };
  CHECK_FALSE(execute(nullptr));
  preserved(shapes, previous);
  fixture.write_bytes(original + "\n/* unaccepted supplier change */\n");
  CHECK_FALSE(execute(&initial));
  StepImportTopologyContext changed(fixture.root);
  CHECK_FALSE(execute(&changed));
  preserved(shapes, previous);
  std::filesystem::rename(fixture.path(), fixture.root / "missing.step");
  StepImportTopologyContext missing(fixture.root);
  CHECK_FALSE(execute(&missing));
  preserved(shapes, previous);
  std::filesystem::rename(fixture.root / "missing.step", fixture.path());
  fixture.write_bytes(original);
  StepImportTopologyContext restored(fixture.root);
  REQUIRE(execute(&restored));
  CHECK(fixture.bytes() == original);
}

TEST_CASE("Reference refresh preserves body identity and rejects Lost or Ambiguous selected bodies",
          "[geometry][assembly-step-reference-part]") {
  Fixture fixture;
  fixture.write(box());
  auto document = fixture.reference();
  const auto definition = *document.reference_part_definition();
  const auto baseline = document.imported_topology_catalogs().front();
  auto shapes = cache();
  StepImportTopologyContext initial(fixture.root);
  REQUIRE(GeometryRecomputeExecutor(&initial).execute_document(document, shapes));
  const auto previous = topology(shapes.reference_result()->shape);
  SECTION("unique surviving face evidence binds the changed owner") {
    fixture.write(box(35));
    accept_current(document, fixture);
    CHECK_FALSE(shapes.validated_reference_result(document));
    StepImportTopologyContext refreshed(fixture.root);
    REQUIRE(GeometryRecomputeExecutor(&refreshed).execute_document(document, shapes));
    CHECK(RectangleExtrusionAdapter{}.summarize(shapes.reference_result()->shape).volume_mm3 ==
          Approx(7000));
    CHECK(document.reference_part_definition()->outputs() == definition.outputs());
    CHECK(document.imported_topology_catalogs().front() == baseline);
  }
  SECTION("removed body is Lost") {
    fixture.write(BRepPrimAPI_MakeBox(gp_Pnt(200, 300, 400), 11, 22, 33).Shape());
    accept_current(document, fixture);
    StepImportTopologyContext refreshed(fixture.root);
    const auto report = checked(refreshed.recover(document, definition.catalog()));
    REQUIRE(report->find(definition.outputs().front().imported_body));
    CHECK(report->find(definition.outputs().front().imported_body)->status ==
          ImportedTopologyRecoveryStatus::Lost);
    CHECK_FALSE(GeometryRecomputeExecutor(&refreshed).execute_document(document, shapes));
    preserved(shapes, previous);
  }
  SECTION("equal alternative bodies are Ambiguous") {
    const auto first = box();
    fixture.write(compound({first, BRepBuilderAPI_Copy(first).Shape()}));
    accept_current(document, fixture);
    StepImportTopologyContext refreshed(fixture.root);
    const auto report = checked(refreshed.recover(document, definition.catalog()));
    REQUIRE(report->find(definition.outputs().front().imported_body));
    CHECK(report->find(definition.outputs().front().imported_body)->status ==
          ImportedTopologyRecoveryStatus::Ambiguous);
    CHECK_FALSE(GeometryRecomputeExecutor(&refreshed).execute_document(document, shapes));
    preserved(shapes, previous);
  }
}

TEST_CASE("Reference conversion seeds the real imported base feature and roundtrips without source "
          "mutation",
          "[geometry][assembly-step-reference-part]") {
  Fixture fixture;
  fixture.write(box());
  const auto original = fixture.bytes();
  const auto reference = fixture.reference();
  const auto editable = checked(reference.convert_reference_to_editable(
      DocumentId("editable"), "Converted", FeatureId("imported.base"), {BodyId("converted.body")}));
  CHECK_FALSE(editable.is_reference_part());
  REQUIRE(editable.imported_body_features().size() == 1);
  const auto& base = editable.imported_body_features().front();
  auto shapes = cache();
  StepImportTopologyContext context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor(&context).execute_document(reference, shapes));
  auto converted = checked(GeometryRecomputeExecutor(&context).execute_document(editable, shapes));
  CHECK(converted.executed_feature_count == 1);
  CHECK(shapes.reference_result() == nullptr);
  CHECK(shapes.feature_shape_count() == 1);
  CHECK(shapes.body_shape_count() == 1);
  REQUIRE(shapes.find_feature_shape(base.id()));
  REQUIRE(shapes.find_body_result(BodyId("converted.body")));
  CHECK(shapes.find_body_result(BodyId("converted.body"))->source_feature_id == base.id());
  REQUIRE(shapes.final_shape());
  CHECK(shapes.final_feature_id() == base.id());
  CHECK(RectangleExtrusionAdapter{}.summarize(*shapes.final_shape()).volume_mm3 == Approx(6000));
  const auto old_feature = topology(*shapes.find_feature_shape(base.id()));
  const auto old_body = topology(*shapes.find_body_shape(BodyId("converted.body")));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(editable, shapes));
  CHECK(topology(*shapes.find_feature_shape(base.id())) == old_feature);
  CHECK(topology(*shapes.find_body_shape(BodyId("converted.body"))) == old_body);
  fixture.write_bytes(original + "\n/* unaccepted change */\n");
  StepImportTopologyContext stale(fixture.root);
  CHECK_FALSE(GeometryRecomputeExecutor(&stale).execute_document(editable, shapes));
  CHECK(topology(*shapes.find_feature_shape(base.id())) == old_feature);
  CHECK(topology(*shapes.find_body_shape(BodyId("converted.body"))) == old_body);
  fixture.write_bytes(original);
  const auto canonical = checked(serialize_part_document_to_json(editable));
  const auto reopened = checked(deserialize_part_document_from_json(canonical));
  CHECK(checked(serialize_part_document_to_json(reopened)) == canonical);
  auto reopened_cache = cache();
  StepImportTopologyContext reopened_context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor(&reopened_context).execute_document(reopened, reopened_cache));
  CHECK(reopened_cache.find_body_result(BodyId("converted.body"))->source_feature_id == base.id());
  CHECK(fixture.bytes() == original);
}

TEST_CASE(
    "Reference endpoints enter real local and hierarchy equations with explicit source context",
    "[geometry][assembly-step-reference-part][step-reference-equations]") {
  Fixture fixture;
  fixture.write(BRepPrimAPI_MakeCylinder(5, 30).Shape());
  const auto original = fixture.bytes();
  const auto document = fixture.reference();
  const auto& catalog = document.imported_topology_catalogs().front();
  const auto spelling = [&](ImportedGeometryClass geometry, ImportedTopologyCapability capability) {
    const auto entry =
        std::find_if(catalog.entries().begin(), catalog.entries().end(),
                     [&](const auto& value) { return value.descriptor.geometry == geometry; });
    REQUIRE(entry != catalog.entries().end());
    return checked(make_imported_topology_target_spelling(
        checked(ImportedTopologyReference::create(catalog.id(), entry->id, capability))));
  };
  const auto point = spelling(ImportedGeometryClass::Circle, ImportedTopologyCapability::Point);
  const auto plane = spelling(ImportedGeometryClass::Plane, ImportedTopologyCapability::Plane);
  const auto axis = spelling(ImportedGeometryClass::Cylinder, ImportedTopologyCapability::Axis);
  auto root = checked(AssemblyDocument::create(DocumentId("root"), "Root"));
  REQUIRE(root.add_member_part(document.id()));
  for (const auto& [name, transform] :
       std::vector<std::pair<std::string, RigidTransform>>{{"a", {}}, {"b", {{10, 20, 30}, {}}}})
    REQUIRE(root.add_component_instance(checked(ComponentInstance::create(
        ComponentInstanceId(name), name, document.id(), ComponentVisibility::Visible,
        ComponentSuppressionState::Active, ComponentGroundingState::Free, transform))));
  auto child = checked(AssemblyDocument::create(DocumentId("child"), "Child"));
  REQUIRE(child.add_member_part(document.id()));
  REQUIRE(child.add_component_instance(checked(ComponentInstance::create(
      ComponentInstanceId("nested"), "Nested", document.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Free,
      RigidTransform{{1, 2, 3}, {90, 0, 0}}))));
  REQUIRE(root.add_subassembly_instance(checked(SubassemblyInstance::create(
      SubassemblyInstanceId("outer"), "Outer", child.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, RigidTransform{{100, 200, 300}, {0, 0, 90}}))));
  auto project = checked(Project::create(DocumentId("project"), "Project", root));
  REQUIRE(project.add_part_document(document));
  REQUIRE(project.add_child_assembly_document(child));
  const auto local = [](const char* component, const std::string& reference) {
    return checked(AssemblyConstraintTarget::create(ComponentInstanceId(component), reference));
  };
  const auto hierarchy = [](const char* component, std::vector<SubassemblyInstanceId> path,
                            const std::string& reference) {
    return checked(AssemblyHierarchyConstraintEndpoint::create(
        std::move(path), ComponentInstanceId(component), reference));
  };
  StepImportTopologyContext context(fixture.root);
  const auto plain_datum =
      checked(make_assembly_reference_target_spelling(DatumPlaneId("datum.xy")));
  const auto explicit_frame =
      checked(make_assembly_frame_target_spelling(DatumPlaneId("datum.xy")));
  const auto plain = checked(AssemblyConstraintTargetResolver{&context}.resolve_geometric(
      project, local("a", plain_datum)));
  const auto frame = checked(AssemblyConstraintTargetResolver{&context}.resolve_geometric(
      project, local("a", explicit_frame)));
  CHECK(plain.source_kind == AssemblyGeometricTargetSourceKind::DatumPlane);
  CHECK(plain.capabilities == std::vector{AssemblyGeometricTargetCapability::Plane});
  CHECK_FALSE(project_frame(plain));
  CHECK(frame.source_kind == AssemblyGeometricTargetSourceKind::DatumPlaneFrame);
  CHECK(to_string(frame.source_kind) == "DatumPlaneFrame");
  CHECK_FALSE(frame.source_metadata.source_feature);
  CHECK_FALSE(frame.source_metadata.imported_topology);
  CHECK(frame.capabilities == std::vector{AssemblyGeometricTargetCapability::Plane,
                                          AssemblyGeometricTargetCapability::Axis,
                                          AssemblyGeometricTargetCapability::Frame});
  const auto exact_frame = checked(project_frame(frame));
  CHECK(exact_frame.origin == Point3{});
  CHECK(exact_frame.x_axis == Vector3{1, 0, 0});
  CHECK(exact_frame.y_axis == Vector3{0, 1, 0});
  CHECK(exact_frame.z_axis == Vector3{0, 0, 1});
  auto invalid_frame = frame;
  std::get<AssemblyFrameTargetDescriptor>(invalid_frame.descriptor).y_axis = {0, -1, 0};
  CHECK_FALSE(validate_resolved_geometric_target(invalid_frame));
  invalid_frame = frame;
  std::get<AssemblyFrameTargetDescriptor>(invalid_frame.descriptor).origin.x =
      std::numeric_limits<double>::infinity();
  CHECK_FALSE(validate_resolved_geometric_target(invalid_frame));
  invalid_frame = frame;
  invalid_frame.endpoint =
      AssemblyLocalGeometricTargetEndpointIdentity{ComponentInstanceId("a"), plain_datum};
  CHECK_FALSE(validate_resolved_geometric_target(invalid_frame));
  const auto presentations = checked(AssemblyInteractionGeometry::targets(project, &context));
  CHECK(std::any_of(presentations.begin(), presentations.end(), [&](const auto& target) {
    return target.reference.semantic_reference() == plain_datum &&
           target.resolved.source_kind == AssemblyGeometricTargetSourceKind::DatumPlane;
  }));
  CHECK(std::any_of(presentations.begin(), presentations.end(), [&](const auto& target) {
    return target.reference.semantic_reference() == explicit_frame &&
           target.resolved.source_kind == AssemblyGeometricTargetSourceKind::DatumPlaneFrame;
  }));
  const auto joint = checked(AssemblyJoint::create(
      AssemblyJointId("spherical"), "Spherical", AssemblyJointType::Spherical, local("a", point),
      local("b", point), AssemblyJointState::Active, std::vector<AssemblyJointCoordinateSlot>{}));
  CHECK_FALSE(AssemblySphericalJointEquationBuilder{}.build(project, joint));
  const auto equation =
      checked(AssemblySphericalJointEquationBuilder{&context}.build(project, joint));
  CHECK(equation.residual.center_offset_mm == Vector3{10, 20, 30});
  CHECK(equation.target_a.target.source_metadata.imported_topology);
  CHECK_FALSE(equation.target_a.target.source_metadata.source_feature);
  const auto cross_joint = checked(AssemblyHierarchyJoint::create(
      AssemblyJointId("cross.spherical"), "Cross spherical", AssemblyJointType::Spherical,
      hierarchy("a", {}, point), hierarchy("nested", {SubassemblyInstanceId("outer")}, point),
      AssemblyJointState::Active, std::vector<AssemblyJointCoordinateSlot>{}));
  CHECK_FALSE(AssemblyHierarchySphericalJointEquationBuilder{}.build(project, cross_joint));
  const auto cross =
      checked(AssemblyHierarchySphericalJointEquationBuilder{&context}.build(project, cross_joint));
  const auto* point_entry =
      catalog.find(checked(parse_imported_topology_target_spelling(point)).entry());
  REQUIRE(point_entry);
  const double z = point_entry->descriptor.origin.z;
  CHECK(cross.target_b.equation_space_point.x == Approx(98 + z).margin(1e-7));
  CHECK(cross.target_b.equation_space_point.y == Approx(201).margin(1e-7));
  CHECK(cross.target_b.equation_space_point.z == Approx(303).margin(1e-7));
  const auto relationship = [&](AssemblyConstraintType type, const std::string& target) {
    return checked(AssemblyConstraint::create(AssemblyConstraintId("relationship"), "Relationship",
                                              type, local("a", target), local("b", target)));
  };
  const auto mate = relationship(AssemblyConstraintType::Mate, plane);
  const auto concentric = relationship(AssemblyConstraintType::Concentric, axis);
  const auto coincident = relationship(AssemblyConstraintType::Coincident, point);
  const auto missing_context = AssemblyConstraintEquationBuilder{}.build(project, mate);
  REQUIRE_FALSE(missing_context);
  CHECK(missing_context.error().message().find("explicit verified source context") !=
        std::string::npos);
  CHECK(AssemblyConstraintEquationBuilder{&context}.build(project, mate));
  CHECK(AssemblyConcentricConstraintEquationBuilder{&context}.build(project, concentric));
  CHECK(AssemblyGenericRelationshipEquationBuilder{&context}.build(project, coincident));
  for (const auto& [type, target] : std::vector<std::pair<AssemblyConstraintType, std::string>>{
           {AssemblyConstraintType::Mate, plane},
           {AssemblyConstraintType::Concentric, axis},
           {AssemblyConstraintType::Coincident, point}}) {
    const auto query = checked(AssemblyHierarchyConstraintQuery::create(
        AssemblyConstraintId("cross.relationship"), type, hierarchy("a", {}, target),
        hierarchy("nested", {SubassemblyInstanceId("outer")}, target)));
    CHECK_FALSE(AssemblyHierarchyConstraintEquationBuilder{}.build(project, query));
    CHECK(AssemblyHierarchyConstraintEquationBuilder{&context}.build(project, query));
  }
  fixture.write_bytes(original + "\n/* changed after equations */\n");
  CHECK_FALSE(AssemblySphericalJointEquationBuilder{&context}.build(project, joint));
  CHECK_FALSE(AssemblyConstraintEquationBuilder{&context}.build(project, mate));
  fixture.write_bytes(original);
  CHECK(fixture.bytes() == original);
}
