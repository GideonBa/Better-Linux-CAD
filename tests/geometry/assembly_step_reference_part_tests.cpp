#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/assembly_reference_target.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/assembly_contact_analyzer.hpp"
#include "blcad/geometry/assembly_cross_hierarchy_constraint_solver.hpp"
#include "blcad/geometry/assembly_cross_hierarchy_joint_motion_solver.hpp"
#include "blcad/geometry/assembly_flexible_subassembly_solver.hpp"
#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"
#include "blcad/geometry/assembly_interference_analyzer.hpp"
#include "blcad/geometry/assembly_joint_motion_solver.hpp"
#include "blcad/geometry/assembly_revolute_sweep_analyzer.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"
#include "blcad/geometry/assembly_step_exporter.hpp"
#include "blcad/geometry/assembly_structured_step_exporter.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/geometry/step_import_topology.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/geometry/step_reference_commands.hpp"
#include "blcad/geometry/viewport_scene.hpp"

#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Builder.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <STEPCAFControl_Controller.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Compound.hxx>
#include <UnitsMethods_LengthUnit.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <set>
#include <unistd.h>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;
namespace {
template <class T> T value(Result<T> result) {
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
std::string json(const Project& project) {
  return value(serialize_project_to_json(project));
}
Quantity angle(double degrees) {
  return value(Quantity::angle_deg(degrees));
}
std::string frame() {
  return value(make_assembly_frame_target_spelling(DatumPlaneId("datum.xy")));
}
AssemblyConstraintTarget target(const char* component, const std::string& semantic) {
  return value(AssemblyConstraintTarget::create(ComponentInstanceId(component), semantic));
}
AssemblyHierarchyConstraintEndpoint endpoint(std::vector<SubassemblyInstanceId> path,
                                             const char* component, const std::string& semantic) {
  return value(AssemblyHierarchyConstraintEndpoint::create(
      std::move(path), ComponentInstanceId(component), semantic));
}
ComponentInstance occurrence(const char* component, ComponentGroundingState grounding,
                             RigidTransform transform = identity_rigid_transform()) {
  return value(ComponentInstance::create(ComponentInstanceId(component), component,
                                         DocumentId("part.reference"), ComponentVisibility::Visible,
                                         ComponentSuppressionState::Active, grounding, transform));
}
SubassemblyInstance boundary(const char* id, const char* assembly, RigidTransform transform) {
  return value(SubassemblyInstance::create(SubassemblyInstanceId(id), id, DocumentId(assembly),
                                           ComponentVisibility::Visible,
                                           ComponentSuppressionState::Active, transform));
}
void coincident(AssemblyDocument& assembly, const char* id, const char* first, const char* second,
                const std::string& semantic) {
  REQUIRE(assembly.add_constraint(value(
      AssemblyConstraint::create(AssemblyConstraintId(id), id, AssemblyConstraintType::Coincident,
                                 target(first, semantic), target(second, semantic)))));
}
std::vector<SubassemblyInstanceId> nested_path() {
  return {SubassemblyInstanceId("outer.left"), SubassemblyInstanceId("inner")};
}
void near(Point3 a, Point3 b) {
  CHECK(a.x == Approx(b.x).margin(1e-5));
  CHECK(a.y == Approx(b.y).margin(1e-5));
  CHECK(a.z == Approx(b.z).margin(1e-5));
}
Point3 world_point(const Project& project, const char* component,
                   const std::vector<SubassemblyInstanceId>& path, Point3 local) {
  auto leaves = value(AssemblyLeafOccurrenceResolver{}.resolve(project));
  auto leaf = std::find_if(leaves.begin(), leaves.end(), [&](const auto& item) {
    return item.component_instance == ComponentInstanceId(component) &&
           item.subassembly_path == path;
  });
  REQUIRE(leaf != leaves.end());
  return AssemblyHierarchyTransformEvaluator{}.evaluate_point(leaf->transforms_inner_to_outer,
                                                              local);
}

constexpr auto supplier_literal_name = "Supplier product_definition TDF_Label TopoDS_Shape #42";

std::string file_bytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  REQUIRE(input.good());
  return {std::istreambuf_iterator<char>(input), {}};
}

void write_supplier_shape(const std::filesystem::path& path, const TopoDS_Shape& shape,
                          const char* name = "Revised supplier") {
  Handle(TDocStd_Document) document = new TDocStd_Document("BinXCAF");
  XCAFDoc_DocumentTool::Set(document->Main());
  XCAFDoc_DocumentTool::SetLengthUnit(document, 1, UnitsMethods_LengthUnit_Millimeter);
  const auto tool = XCAFDoc_DocumentTool::ShapeTool(document->Main());
  const auto product = tool->AddShape(shape, false, false);
  REQUIRE(!product.IsNull());
  TDataStd_Name::Set(product, TCollection_ExtendedString(name, true));
  STEPCAFControl_Controller::Init();
  const std::string previous_unit = Interface_Static::CVal("write.step.unit");
  REQUIRE(Interface_Static::SetCVal("write.step.unit", "MM"));
  STEPCAFControl_Writer writer;
  const bool transferred = writer.Transfer(document, STEPControl_AsIs);
  Interface_Static::SetCVal("write.step.unit", previous_unit.c_str());
  REQUIRE(transferred);
  REQUIRE(writer.Write(path.c_str()) == IFSelect_RetDone);
}

struct Fixture {
  std::filesystem::path root;
  StepImportSource source;
  ImportedTopologyCatalog catalog;
  PartDocument part;
  explicit Fixture(bool named_source = false)
      : root(directory()), source(write_source(named_source)), catalog(capture()),
        part(value(StepReferenceCommands{}.create_definition(
            source, catalog, DocumentId("part.reference"), "Shared Reference", root))) {}
  ~Fixture() {
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }
  static std::filesystem::path directory() {
    static std::atomic<unsigned> sequence{};
    auto path = std::filesystem::temp_directory_path() /
                ("blcad-assembly-reference-" + std::to_string(::getpid()) + "-" +
                 std::to_string(++sequence));
    REQUIRE(std::filesystem::create_directories(path));
    return path;
  }
  StepImportSource write_source(bool named_source) const {
    auto shape = value(RectangleExtrusionAdapter{}.make_extruded_rectangle(
        value(Quantity::length_mm(20)), value(Quantity::length_mm(10)),
        value(Quantity::length_mm(5))));
    if (named_source) {
      const auto* topology = OcctShapeView::get(shape);
      REQUIRE(topology);
      write_supplier_shape(root / "supplier.step", *topology, supplier_literal_name);
    } else
      REQUIRE(StepExporter{}.write_step(shape, (root / "supplier.step").string()));
    auto report = value(inspect_step_import_asset(root, "supplier.step"));
    REQUIRE(report.observed_sha256_digest());
    return value(StepImportSource::create(StepImportSourceId("source.supplier"), "Supplier",
                                          "supplier.step", *report.observed_sha256_digest(),
                                          StepImportMode::Reference));
  }
  ImportedTopologyCatalog capture() const {
    return value(StepImportTopology{}.create_catalog(ImportedTopologyCatalogId("catalog.reference"),
                                                     value(StepImportReader{}.read(source, root)),
                                                     0));
  }
  const ImportedTopologyEntry& point_entry() const {
    auto found =
        std::find_if(catalog.entries().begin(), catalog.entries().end(), [](const auto& entry) {
          const auto& d = entry.descriptor;
          return d.kind == ImportedTopologyKind::Vertex && std::abs(d.centroid.x - 10) < 1e-7 &&
                 std::abs(d.centroid.y - 5) < 1e-7 && std::abs(d.centroid.z - 5) < 1e-7;
        });
    REQUIRE(found != catalog.entries().end());
    return *found;
  }
  std::string point() const {
    return value(make_imported_topology_target_spelling(value(ImportedTopologyReference::create(
        catalog.id(), point_entry().id, ImportedTopologyCapability::Point))));
  }
  Project pair(RigidTransform moving = identity_rigid_transform()) const {
    auto assembly = value(AssemblyDocument::create(DocumentId("assembly.root"), "Root"));
    REQUIRE(assembly.add_member_part(part.id()));
    REQUIRE(
        assembly.add_component_instance(occurrence("fixed", ComponentGroundingState::Grounded)));
    REQUIRE(assembly.add_component_instance(
        occurrence("moving", ComponentGroundingState::Free, moving)));
    auto project =
        value(Project::create(DocumentId("project.reference"), "Reference Project", assembly));
    REQUIRE(project.add_part_document(part));
    return project;
  }
  Project local_solve() const {
    auto project = pair({{17, 12, 4}, {}});
    coincident(project.assembly(), "point.local", "fixed", "moving", point());
    return project;
  }
  Project local_motion() const {
    auto project = pair();
    REQUIRE(project.assembly().add_joint(value(AssemblyJoint::create(
        AssemblyJointId("joint.local"), "Explicit origin Frame", AssemblyJointType::Revolute,
        target("fixed", frame()), target("moving", frame()), AssemblyJointState::Active, angle(-60),
        angle(60), angle(0)))));
    return project;
  }
  Project nested(bool flexible = false, bool noncommuting = true) const {
    auto project = pair();
    const auto outer =
        noncommuting ? RigidTransform{{17, 19, 23}, {0, 0, 90}} : identity_rigid_transform();
    const auto inner =
        noncommuting ? RigidTransform{{7, 11, 13}, {90, 0, 0}} : identity_rigid_transform();
    auto grand = value(AssemblyDocument::create(DocumentId("assembly.grand"), "Grandchild"));
    REQUIRE(grand.add_member_part(part.id()));
    REQUIRE(grand.add_component_instance(occurrence(
        "nested", ComponentGroundingState::Free,
        noncommuting ? RigidTransform{{3, 4, 5}, {0, 90, 0}} : identity_rigid_transform())));
    auto child = value(AssemblyDocument::create(DocumentId("assembly.child"), "Child"));
    REQUIRE(child.add_subassembly_instance(boundary("inner", "assembly.grand", inner)));
    if (flexible) {
      REQUIRE(child.add_member_part(part.id()));
      REQUIRE(child.add_component_instance(
          occurrence("child.fixed", ComponentGroundingState::Grounded)));
      REQUIRE(child.add_component_instance(
          occurrence("child.moving", ComponentGroundingState::Free, {{5, 3, 8}, {}})));
      coincident(child, "point.child", "child.fixed", "child.moving", point());
    }
    REQUIRE(project.add_child_assembly_document(grand));
    REQUIRE(project.add_child_assembly_document(child));
    REQUIRE(project.assembly().add_subassembly_instance(
        boundary("outer.left", "assembly.child", outer)));
    REQUIRE(project.assembly().add_subassembly_instance(
        boundary("outer.right", "assembly.child", {{100, 50, 20}, {0, 0, -90}})));
    REQUIRE(project.validate_assembly_structure());
    return project;
  }
  Project cross_solve() const {
    auto project = nested();
    REQUIRE(project.add_cross_hierarchy_constraint(value(AssemblyHierarchyConstraint::create(
        AssemblyConstraintId("point.cross"), "Root to grandchild imported point",
        AssemblyConstraintType::Coincident, endpoint({}, "fixed", point()),
        endpoint(nested_path(), "nested", point())))));
    return project;
  }
  Project cross_motion() const {
    auto project = nested(false, false);
    REQUIRE(project.add_cross_hierarchy_joint(value(AssemblyHierarchyJoint::create(
        AssemblyJointId("joint.cross"), "Root to grandchild explicit origin Frame",
        AssemblyJointType::Revolute, endpoint({}, "fixed", frame()),
        endpoint(nested_path(), "nested", frame()), AssemblyJointState::Active, angle(-60),
        angle(60), angle(0)))));
    return project;
  }
  void change_source() const {
    std::ifstream input(root / "supplier.step", std::ios::binary);
    std::string bytes(std::istreambuf_iterator<char>(input), {});
    auto end = bytes.find("ENDSEC;");
    REQUIRE(end != std::string::npos);
    bytes.insert(end, "/* external supplier revision B */\n");
    input.close();
    std::ofstream output(root / "supplier.step", std::ios::binary | std::ios::trunc);
    output << bytes;
    REQUIRE(output.good());
  }
};
AssemblyCrossHierarchySolveGroup cross_group(const Project& project) {
  auto graph = value(AssemblyCrossHierarchyConstraintGraph::build(project));
  REQUIRE(graph.solve_group_count() == 1);
  return graph.solve_groups().front();
}

Project command_reference_project(const Fixture& fixture) {
  const auto inspected = value(inspect_step_import_source(fixture.source, fixture.root));
  REQUIRE(inspected.state() == StepImportFreshness::Available);
  const auto imported = value(StepImportReader{}.read(fixture.source, fixture.root));
  REQUIRE(imported.products.size() == 1);
  REQUIRE(imported.body_count() == 1);
  CHECK(value(StepImportTopology{}.create_catalog(fixture.catalog.id(), imported, 0)) ==
        fixture.catalog);
  auto assembly = value(AssemblyDocument::create(DocumentId("assembly.root"), "Imported workflow"));
  auto project = value(Project::create(DocumentId("project.reference"), "Reference acceptance",
                                       std::move(assembly)));
  REQUIRE(project.add_step_import_source(fixture.source));
  REQUIRE(project.add_imported_topology_catalog(fixture.catalog));
  REQUIRE(project.add_child_assembly_document(
      value(AssemblyDocument::create(DocumentId("assembly.grand"), "Shared leaf assembly"))));
  auto child = value(AssemblyDocument::create(DocumentId("assembly.child"), "Nested carrier"));
  REQUIRE(
      child.add_subassembly_instance(boundary("inner", "assembly.grand", {{3, 7, 2}, {90, 0, 0}})));
  REQUIRE(project.add_child_assembly_document(std::move(child)));
  REQUIRE(project.assembly().add_subassembly_instance(
      boundary("outer.left", "assembly.child", {{45, 12, 3}, {20, 0, 25}})));
  REQUIRE(project.assembly().add_subassembly_instance(
      boundary("outer.right", "assembly.child", {{-55, 30, 12}, {0, 30, -40}})));

  const StepReferenceCommands commands;
  const auto initial = json(project);
  auto inserted = value(commands.insert(project, fixture.catalog.id(), fixture.part.id(),
                                        imported.products.front().name, project.assembly().id(),
                                        ComponentInstanceId("fixed"), identity_rigid_transform(),
                                        fixture.root));
  CHECK(json(project) == initial);
  project = std::move(inserted.project);
  project = value(commands.repeat(project, fixture.part.id(), project.assembly().id(),
                                  ComponentInstanceId("moving"), identity_rigid_transform(),
                                  fixture.root))
                .project;
  project =
      value(commands.repeat(project, fixture.part.id(), DocumentId("assembly.grand"),
                            ComponentInstanceId("nested"), {{3, 4, 5}, {0, 90, 0}}, fixture.root))
          .project;
  REQUIRE(project.assembly().set_component_instance_grounding_state(
      ComponentInstanceId("fixed"), ComponentGroundingState::Grounded));
  const auto plane = std::find_if(
      fixture.catalog.entries().begin(), fixture.catalog.entries().end(), [](const auto& entry) {
        const auto& d = entry.descriptor;
        return d.kind == ImportedTopologyKind::Face && d.geometry == ImportedGeometryClass::Plane &&
               std::abs(d.centroid.z - 5) < 1e-7 && std::abs(d.direction.z - 1) < 1e-7;
      });
  REQUIRE(plane != fixture.catalog.entries().end());
  const auto semantic =
      value(make_imported_topology_target_spelling(value(ImportedTopologyReference::create(
          fixture.catalog.id(), plane->id, ImportedTopologyCapability::Plane))));
  // Co-directed imported planes remain parallel throughout motion about the
  // explicitly authored origin Frame. No Frame is inferred from the STEP face.
  REQUIRE(project.assembly().add_constraint(value(AssemblyConstraint::create(
      AssemblyConstraintId("plane.parallel"), "Imported top planes",
      AssemblyConstraintType::Parallel, target("fixed", semantic), target("moving", semantic)))));
  REQUIRE(project.assembly().add_joint(value(AssemblyJoint::create(
      AssemblyJointId("joint.local"), "Explicit origin Frame", AssemblyJointType::Revolute,
      target("fixed", frame()), target("moving", frame()), AssemblyJointState::Active, angle(-60),
      angle(60), angle(0)))));
  REQUIRE(project.validate_assembly_structure());
  REQUIRE(project.part_document_count() == 1);
  const auto leaves = value(AssemblyLeafOccurrenceResolver{}.resolve(project));
  REQUIRE(leaves.size() == 4);
  for (const auto& leaf : leaves)
    CHECK(leaf.referenced_part_document == fixture.part.id());
  return project;
}

using PhysicalRecord = std::array<double, 11>;

std::vector<PhysicalRecord> box_occurrence_oracle(const Project& project) {
  std::vector<PhysicalRecord> result;
  for (const auto& leaf : value(AssemblyLeafOccurrenceResolver{}.resolve(project))) {
    PhysicalRecord record{
        std::numeric_limits<double>::infinity(),  std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),  -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
    for (const double x : {-10.0, 10.0})
      for (const double y : {-5.0, 5.0})
        for (const double z : {0.0, 5.0}) {
          const auto point = AssemblyHierarchyTransformEvaluator{}.evaluate_point(
              leaf.transforms_inner_to_outer, {x, y, z});
          record[0] = std::min(record[0], point.x);
          record[1] = std::min(record[1], point.y);
          record[2] = std::min(record[2], point.z);
          record[3] = std::max(record[3], point.x);
          record[4] = std::max(record[4], point.y);
          record[5] = std::max(record[5], point.z);
        }
    const auto center = AssemblyHierarchyTransformEvaluator{}.evaluate_point(
        leaf.transforms_inner_to_outer, {0, 0, 2.5});
    record[6] = center.x;
    record[7] = center.y;
    record[8] = center.z;
    record[9] = 1000;
    record[10] = 700;
    result.push_back(record);
  }
  std::sort(result.begin(), result.end());
  return result;
}

std::vector<PhysicalRecord> physical_solids(const std::vector<GeometryShape>& shapes) {
  std::vector<PhysicalRecord> result;
  for (const auto& shape : shapes) {
    const auto* topology = OcctShapeView::get(shape);
    REQUIRE(topology);
    TopTools_IndexedMapOfShape solids;
    TopExp::MapShapes(*topology, TopAbs_SOLID, solids);
    for (int index = 1; index <= solids.Extent(); ++index) {
      PhysicalRecord record{};
      Bnd_Box bounds;
      BRepBndLib::AddOptimal(solids(index), bounds, false, false);
      bounds.Get(record[0], record[1], record[2], record[3], record[4], record[5]);
      GProp_GProps properties;
      BRepGProp::VolumeProperties(solids(index), properties);
      const auto center = properties.CentreOfMass();
      record[6] = center.X();
      record[7] = center.Y();
      record[8] = center.Z();
      record[9] = properties.Mass();
      BRepGProp::SurfaceProperties(solids(index), properties);
      record[10] = properties.Mass();
      result.push_back(record);
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}

void same_physical_solids(const std::vector<PhysicalRecord>& actual,
                          const std::vector<PhysicalRecord>& expected) {
  REQUIRE(actual.size() == expected.size());
  for (std::size_t index = 0; index < actual.size(); ++index)
    for (std::size_t field = 0; field < actual[index].size(); ++field) {
      CAPTURE(index, field);
      CHECK(actual[index][field] == Approx(expected[index][field]).margin(1e-5));
    }
}

std::vector<PhysicalRecord> exported_solids(const std::filesystem::path& root,
                                            const char* filename) {
  const auto source = value(StepImportSource::create(
      StepImportSourceId("readback"), "Re-exported result", filename,
      value(step_import_sha256(file_bytes(root / filename))), StepImportMode::Reference));
  const auto imported = value(StepImportReader{}.read(source, root));
  return physical_solids(value(StepImportReader{}.preview_shapes(imported)));
}

void check_persistent_identity_fields(const nlohmann::json& node) {
  // Literal foreign names may contain these words. Only raw identity fields,
  // runtime indices and kernel payload records are forbidden in persistence.
  static const std::set<std::string> forbidden{
      "definition_key", "product_definition", "parent_definition", "product_index",
      "body_index",     "occurrence_index",   "tdf_label",         "xde_label",
      "topods_shape",   "occt_shape",         "brep_payload",      "step_payload"};
  if (node.is_object())
    for (const auto& item : node.items()) {
      INFO(item.key());
      CHECK_FALSE(forbidden.contains(item.key()));
      check_persistent_identity_fields(item.value());
    }
  else if (node.is_array())
    for (const auto& item : node)
      check_persistent_identity_fields(item);
}
} // namespace

TEST_CASE("Reference assembly geometry shares one definition through exact repeated nested "
          "transform chains",
          "[geometry][assembly-step-reference-part]") {
  Fixture fixture;
  const auto project = fixture.nested();
  const auto before = json(project);
  REQUIRE(project.part_document_count() == 1);
  const auto leaves = value(AssemblyLeafOccurrenceResolver{}.resolve(project));
  REQUIRE(leaves.size() == 4);
  for (const auto& leaf : leaves)
    CHECK(leaf.referenced_part_document == fixture.part.id());
  near(world_point(project, "nested", nested_path(), fixture.point_entry().descriptor.centroid),
       {1, 34, 45});
  StepImportTopologyContext context(fixture.root);
  const auto scene = value(ViewportSceneBuilder{&context}.build_project(project));
  CHECK(scene.size() == 4);
  const auto contacts = value(AssemblyContactAnalyzer{&context}.analyze(project));
  CHECK(contacts.component_occurrence_count == 4);
  CHECK(contacts.recomputed_part_count == 1);
  CHECK(contacts.evaluated_pair_count == 6);
  CHECK(json(project) == before);
  const auto saved = fixture.root / "assembly.blcad.project.json";
  REQUIRE(write_project_json_file(project, saved));
  const auto reopened = value(read_project_json_file(saved));
  CHECK(json(reopened) == before);
  StepImportTopologyContext reopened_context(fixture.root);
  CHECK(value(ViewportSceneBuilder{&reopened_context}.build_project(reopened)).size() == 4);
  near(world_point(reopened, "nested", nested_path(), fixture.point_entry().descriptor.centroid),
       {1, 34, 45});
}

TEST_CASE("Reference imported points solve local and occurrence-qualified relationships without "
          "feature producers",
          "[geometry][assembly-step-reference-part]") {
  Fixture fixture;
  auto local = fixture.local_solve();
  const auto before = json(local);
  const auto part_before = value(serialize_part_document_to_json(fixture.part));
  StepImportTopologyContext context(fixture.root);
  auto solved = value(AssemblyRigidBodySolver{&context}.solve(
      local, {ComponentInstanceId("fixed"), ComponentInstanceId("moving")}));
  REQUIRE(solved.converged());
  CHECK(solved.residual_summary.final_rms < 1e-7);
  CHECK(json(local) == before);
  StepImportTopologyContext apply_context(fixture.root);
  REQUIRE(AssemblySolveResultApplier{&apply_context}.apply(local, solved));
  near(world_point(local, "fixed", {}, fixture.point_entry().descriptor.centroid),
       world_point(local, "moving", {}, fixture.point_entry().descriptor.centroid));
  CHECK(value(serialize_part_document_to_json(*local.find_part_document(fixture.part.id()))) ==
        part_before);
  CHECK(local.find_part_document(fixture.part.id())->features().empty());
  auto cross = fixture.cross_solve();
  const auto cross_before = json(cross);
  const auto root_fixed =
      cross.assembly().find_component_instance(ComponentInstanceId("fixed"))->transform();
  StepImportTopologyContext cross_context(fixture.root);
  auto cross_solved = value(
      AssemblyCrossHierarchyConstraintSolver{&cross_context}.solve(cross, cross_group(cross)));
  REQUIRE(cross_solved.converged());
  CHECK(cross_solved.residual_summary.final_rms < 1e-7);
  CHECK(json(cross) == cross_before);
  StepImportTopologyContext cross_apply(fixture.root);
  REQUIRE(AssemblyCrossHierarchySolveResultApplier{&cross_apply}.apply(cross, cross_solved));
  near(world_point(cross, "fixed", {}, fixture.point_entry().descriptor.centroid),
       world_point(cross, "nested", nested_path(), fixture.point_entry().descriptor.centroid));
  CHECK(cross.assembly().find_component_instance(ComponentInstanceId("fixed"))->transform() ==
        root_fixed);
}

TEST_CASE("Flexible Reference child solving retains reachable grandchildren and repeated shared "
          "authority",
          "[geometry][assembly-step-reference-part]") {
  Fixture fixture;
  auto project = fixture.nested(true);
  const auto before = json(project);
  const auto left_boundary = project.assembly()
                                 .find_subassembly_instance(SubassemblyInstanceId("outer.left"))
                                 ->transform();
  const auto grand_boundary = project.find_assembly_document(DocumentId("assembly.child"))
                                  ->find_subassembly_instance(SubassemblyInstanceId("inner"))
                                  ->transform();
  const auto grand_transform = project.find_assembly_document(DocumentId("assembly.grand"))
                                   ->component_instances()
                                   .front()
                                   .transform();
  StepImportTopologyContext context(fixture.root);
  auto result = value(AssemblyFlexibleSubassemblySolver{&context}.solve(
      project, {SubassemblyInstanceId("outer.left")},
      {ComponentInstanceId("child.fixed"), ComponentInstanceId("child.moving")}));
  REQUIRE(result.converged());
  CHECK(json(project) == before);
  StepImportTopologyContext apply_context(fixture.root);
  REQUIRE(AssemblyFlexibleSubassemblySolveResultApplier{&apply_context}.apply(project, result));
  for (const char* occurrence_id : {"outer.left", "outer.right"}) {
    const std::vector<SubassemblyInstanceId> path{SubassemblyInstanceId(occurrence_id)};
    near(world_point(project, "child.fixed", path, fixture.point_entry().descriptor.centroid),
         world_point(project, "child.moving", path, fixture.point_entry().descriptor.centroid));
  }
  CHECK(project.assembly()
            .find_subassembly_instance(SubassemblyInstanceId("outer.left"))
            ->transform() == left_boundary);
  CHECK(project.find_assembly_document(DocumentId("assembly.child"))
            ->find_subassembly_instance(SubassemblyInstanceId("inner"))
            ->transform() == grand_boundary);
  CHECK(project.find_assembly_document(DocumentId("assembly.grand"))
            ->component_instances()
            .front()
            .transform() == grand_transform);
  StepImportTopologyContext scene_context(fixture.root);
  CHECK(value(ViewportSceneBuilder{&scene_context}.build_project(project)).size() == 8);
}

TEST_CASE(
    "Immutable Reference origin Frames drive local and nested Revolute motion with atomic apply",
    "[geometry][assembly-step-reference-part]") {
  Fixture fixture;
  auto local = fixture.local_motion();
  const auto before = json(local);
  StepImportTopologyContext context(fixture.root);
  auto motion = value(
      AssemblyJointMotionSolver{&context}.move(local, AssemblyJointId("joint.local"), angle(25)));
  REQUIRE(motion.converged());
  CHECK(json(local) == before);
  StepImportTopologyContext apply_context(fixture.root);
  REQUIRE(AssemblyJointMotionResultApplier{&apply_context}.apply(local, motion));
  CHECK(local.assembly().joints().front().coordinate_deg() == Approx(25));
  const auto& transform =
      local.assembly().find_component_instance(ComponentInstanceId("moving"))->transform();
  CHECK(std::abs(transform.rotation_deg.z) == Approx(25).margin(1e-5));
  CHECK(local.find_part_document(fixture.part.id())->is_reference_part());
  auto nested = fixture.cross_motion();
  const auto nested_before = json(nested);
  StepImportTopologyContext nested_context(fixture.root);
  auto nested_motion = value(AssemblyCrossHierarchyJointMotionSolver{&nested_context}.move(
      nested, AssemblyJointId("joint.cross"), angle(15)));
  REQUIRE(nested_motion.converged());
  CHECK(json(nested) == nested_before);
  StepImportTopologyContext nested_apply(fixture.root);
  REQUIRE(
      AssemblyCrossHierarchyJointMotionResultApplier{&nested_apply}.apply(nested, nested_motion));
  CHECK(nested.cross_hierarchy_joints().front().coordinate_deg() == Approx(15));
  CHECK(nested.find_assembly_document(DocumentId("assembly.grand"))
            ->component_instances()
            .front()
            .transform() != identity_rigid_transform());
  CHECK(nested.assembly().component_instances().front().transform() == identity_rigid_transform());
}

TEST_CASE("Reference posed contact interference clearance and sampled sweep use actual shared "
          "imported bodies",
          "[geometry][assembly-step-reference-part]") {
  Fixture fixture;
  auto separated = fixture.pair({{22, 0, 0}, {}});
  const auto before = json(separated);
  StepImportTopologyContext context(fixture.root);
  const auto contacts = value(AssemblyContactAnalyzer{&context}.analyze(separated));
  REQUIRE(contacts.records.size() == 1);
  CHECK(contacts.records.front().classification == AssemblyContactClassification::Separated);
  REQUIRE(contacts.records.front().minimum_distance_mm);
  CHECK(*contacts.records.front().minimum_distance_mm == Approx(2).margin(1e-5));
  const auto clearance = value(AssemblyClearanceAnalyzer{&context}.analyze(separated, {3, 1e-6}));
  REQUIRE(clearance.clearance_violations.size() == 1);
  CHECK(clearance.clearance_violations.front().minimum_distance_mm == Approx(2).margin(1e-5));
  CHECK(clearance.interferences.empty());
  CHECK(clearance.recomputed_part_count == 1);
  CHECK(json(separated) == before);
  auto touching = fixture.pair({{20, 0, 0}, {}});
  CHECK(value(AssemblyContactAnalyzer{&context}.analyze(touching)).records.front().classification ==
        AssemblyContactClassification::Touching);
  auto overlapping = fixture.pair({{10, 0, 0}, {}});
  const auto overlap = value(AssemblyInterferenceAnalyzer{&context}.analyze(overlapping));
  REQUIRE(overlap.interferences.size() == 1);
  CHECK(overlap.interferences.front().overlap_volume_mm3 == Approx(500).margin(1e-5));
  auto motion = fixture.local_motion();
  const auto motion_before = json(motion);
  const AssemblyRevoluteSweepRequest request{
      {AssemblyRevoluteSweepJointScope::RootAssemblyLocal, AssemblyJointId("joint.local")},
      angle(-20),
      angle(20),
      3};
  auto sweep = value(AssemblyRevoluteSweepAnalyzer{&context}.analyze(motion, request));
  REQUIRE(sweep.samples.size() == 3);
  for (std::size_t i = 0; i < sweep.samples.size(); ++i) {
    const auto& sample = sweep.samples[i];
    CHECK(sample.coordinate_deg == Approx(-20 + 20 * static_cast<double>(i)));
    REQUIRE(sample.contact_analysis.records.size() == 1);
    CHECK(sample.contact_analysis.records.front().classification ==
          AssemblyContactClassification::Interfering);
  }
  CHECK(json(motion) == motion_before);
}

TEST_CASE("Fresh contexts reject every Reference solve and motion publication after external "
          "source changes",
          "[geometry][assembly-step-reference-part]") {
  Fixture fixture;
  auto local = fixture.local_solve();
  auto cross = fixture.cross_solve();
  auto flexible = fixture.nested(true);
  auto motion = fixture.local_motion();
  auto cross_motion = fixture.cross_motion();
  const auto local_before = json(local), cross_before = json(cross),
             flexible_before = json(flexible), motion_before = json(motion),
             cross_motion_before = json(cross_motion);
  StepImportTopologyContext query(fixture.root);
  auto solved = value(AssemblyRigidBodySolver{&query}.solve(
      local, {ComponentInstanceId("fixed"), ComponentInstanceId("moving")}));
  auto cross_solved =
      value(AssemblyCrossHierarchyConstraintSolver{&query}.solve(cross, cross_group(cross)));
  auto child_solved = value(AssemblyFlexibleSubassemblySolver{&query}.solve(
      flexible, {SubassemblyInstanceId("outer.left")},
      {ComponentInstanceId("child.fixed"), ComponentInstanceId("child.moving")}));
  auto moved = value(
      AssemblyJointMotionSolver{&query}.move(motion, AssemblyJointId("joint.local"), angle(25)));
  auto cross_moved = value(AssemblyCrossHierarchyJointMotionSolver{&query}.move(
      cross_motion, AssemblyJointId("joint.cross"), angle(15)));
  REQUIRE(solved.converged());
  REQUIRE(cross_solved.converged());
  REQUIRE(child_solved.converged());
  REQUIRE(moved.converged());
  REQUIRE(cross_moved.converged());
  SECTION("Changed accepted bytes") {
    fixture.change_source();
  }
  SECTION("Missing accepted asset") {
    REQUIRE(std::filesystem::remove(fixture.root / "supplier.step"));
  }
  StepImportTopologyContext apply_local(fixture.root), apply_cross(fixture.root),
      apply_child(fixture.root), apply_motion(fixture.root), apply_cross_motion(fixture.root);
  CHECK_FALSE(AssemblySolveResultApplier{&apply_local}.apply(local, solved));
  CHECK_FALSE(AssemblyCrossHierarchySolveResultApplier{&apply_cross}.apply(cross, cross_solved));
  CHECK_FALSE(
      AssemblyFlexibleSubassemblySolveResultApplier{&apply_child}.apply(flexible, child_solved));
  CHECK_FALSE(AssemblyJointMotionResultApplier{&apply_motion}.apply(motion, moved));
  CHECK_FALSE(AssemblyCrossHierarchyJointMotionResultApplier{&apply_cross_motion}.apply(
      cross_motion, cross_moved));
  CHECK(json(local) == local_before);
  CHECK(json(cross) == cross_before);
  CHECK(json(flexible) == flexible_before);
  CHECK(json(motion) == motion_before);
  CHECK(json(cross_motion) == cross_motion_before);
  StepImportTopologyContext analysis(fixture.root);
  CHECK_FALSE(AssemblyContactAnalyzer{&analysis}.analyze(local));
  CHECK_FALSE(AssemblyInterferenceAnalyzer{&analysis}.analyze(local));
  CHECK_FALSE(AssemblyClearanceAnalyzer{&analysis}.analyze(local));
  CHECK_FALSE(ViewportSceneBuilder{&analysis}.build_project(local));
  CHECK_FALSE(AssemblyRevoluteSweepAnalyzer{&analysis}.analyze(
      motion, {{AssemblyRevoluteSweepJointScope::RootAssemblyLocal, AssemblyJointId("joint.local")},
               angle(-10),
               angle(10),
               3}));
}

TEST_CASE("STEP Reference acceptance shares command-created Parts through motion refresh and "
          "physical re-export",
          "[geometry][integration][step-import-mvp][assembly-step-reference-part]") {
  Fixture fixture(true);
  const auto asset = fixture.root / "supplier.step";
  const auto supplier_bytes = file_bytes(asset);
  const StepReferenceCommands commands;
  auto project = command_reference_project(fixture);
  const auto* reference = project.find_part_document(fixture.part.id());
  REQUIRE(reference);
  REQUIRE(reference->reference_part_definition());
  CHECK(reference->name() == supplier_literal_name);
  const auto binding = *reference->reference_part_definition();
  REQUIRE(binding.outputs().size() == 1);
  const auto output = binding.outputs().front();
  CHECK(reference->imported_body_features().empty());
  const auto initial = json(project);
  CHECK(file_bytes(asset) == supplier_bytes);

  auto shapes = value(ShapeCache::create(ShapeCacheId("mvp.reference")));
  StepImportTopologyContext initial_context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&initial_context}.execute_document(*reference, shapes));
  REQUIRE(shapes.reference_result());
  CHECK(shapes.feature_shape_count() == 0);
  CHECK(shapes.body_shape_count() == 0);
  REQUIRE(shapes.reference_result()->bodies.size() == 1);
  CHECK(shapes.find_body_shape(output.body) == &shapes.reference_result()->bodies.front().shape);
  CHECK(shapes.reference_result()->definition == binding);
  auto proposed = value(AssemblyJointMotionSolver{&initial_context}.move(
      project, AssemblyJointId("joint.local"), angle(25)));
  REQUIRE(proposed.converged());
  CHECK(json(project) == initial);
  StepImportTopologyContext motion_context(fixture.root);
  REQUIRE(AssemblyJointMotionResultApplier{&motion_context}.apply(project, proposed));
  CHECK(project.assembly().joints().front().coordinate_deg() == Approx(25));
  CHECK(std::abs(project.assembly()
                     .find_component_instance(ComponentInstanceId("moving"))
                     ->transform()
                     .rotation_deg.z) == Approx(25).margin(1e-5));
  CHECK(project.assembly().constraints().front().target_a().semantic_reference() ==
        project.assembly().constraints().front().target_b().semantic_reference());
  const auto moved = json(project);
  const auto moved_leaves = value(AssemblyLeafOccurrenceResolver{}.resolve(project));
  const auto expected = box_occurrence_oracle(project);
  REQUIRE(expected.size() == 4);
  StepImportTopologyContext analysis_context(fixture.root);
  const auto contacts = value(AssemblyContactAnalyzer{&analysis_context}.analyze(project));
  CHECK(contacts.component_occurrence_count == 4);
  CHECK(contacts.recomputed_part_count == 1);
  CHECK(contacts.evaluated_pair_count == 6);
  const auto root_pair =
      std::find_if(contacts.records.begin(), contacts.records.end(), [](const auto& record) {
        return record.pair.occurrence_a.component_instance == ComponentInstanceId("fixed") &&
               record.pair.occurrence_b.component_instance == ComponentInstanceId("moving");
      });
  REQUIRE(root_pair != contacts.records.end());
  CHECK(root_pair->classification == AssemblyContactClassification::Interfering);
  CHECK(root_pair->overlap_volume_mm3 > 0);
  CHECK(root_pair->overlap_volume_mm3 < 1000);
  same_physical_solids(
      physical_solids({value(AssemblyStepExporter{&analysis_context}.build_posed_shape(project))}),
      expected);
  CHECK(json(project) == moved);
  CHECK(file_bytes(asset) == supplier_bytes);

  const auto saved = fixture.root / "reference.blcad.project.json";
  REQUIRE(write_project_json_file(project, saved));
  auto reopened = value(read_project_json_file(saved));
  CHECK(json(reopened) == moved);
  CHECK(value(AssemblyLeafOccurrenceResolver{}.resolve(reopened)) == moved_leaves);
  check_persistent_identity_fields(nlohmann::json::parse(moved));
  CHECK(moved.find(supplier_literal_name) != std::string::npos);
  CHECK(moved.find(fixture.root.string()) == std::string::npos);

  // Only the supplier changes the asset. An explicit accepted refresh then
  // preserves every selection, output id, relationship and placed occurrence.
  fixture.change_source();
  const auto revised_bytes = file_bytes(asset);
  REQUIRE(revised_bytes != supplier_bytes);
  CHECK_FALSE(commands.repeat(reopened, fixture.part.id(), reopened.assembly().id(),
                              ComponentInstanceId("unaccepted"), identity_rigid_transform(),
                              fixture.root));
  CHECK(json(reopened) == moved);
  const auto observed = value(inspect_step_import_source(fixture.source, fixture.root));
  REQUIRE(observed.state() == StepImportFreshness::Changed);
  REQUIRE(observed.observed_sha256_digest());
  auto refreshed = value(commands.refresh(reopened, fixture.part.id(), fixture.root));
  CHECK(json(reopened) == moved);
  CHECK(refreshed.affected_owners.size() == 2);
  project = std::move(refreshed.project);
  reference = project.find_part_document(fixture.part.id());
  REQUIRE(reference);
  CHECK(reference->name() == supplier_literal_name);
  CHECK(*reference->reference_part_definition() == binding);
  CHECK(reference->imported_topology_catalogs() == std::vector{fixture.catalog});
  CHECK(project.imported_topology_catalogs() == std::vector{fixture.catalog});
  REQUIRE(project.find_step_import_source(fixture.source.id()));
  REQUIRE(reference->find_step_import_source(fixture.source.id()));
  const auto accepted = *reference->find_step_import_source(fixture.source.id());
  CHECK(accepted.sha256_digest() == *observed.observed_sha256_digest());
  CHECK(*project.find_step_import_source(fixture.source.id()) == accepted);
  CHECK(accepted.sha256_digest() != fixture.source.sha256_digest());
  CHECK(value(AssemblyLeafOccurrenceResolver{}.resolve(project)) == moved_leaves);
  CHECK(project.assembly().joints().front().coordinate_deg() == Approx(25));
  CHECK(project.assembly().constraints().front().target_a() ==
        reopened.assembly().constraints().front().target_a());
  StepImportTopologyContext refreshed_context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&refreshed_context}.execute_document(*reference, shapes));
  CHECK(value(shapes.validated_reference_result(*reference))->definition == binding);
  const auto refreshed_contacts =
      value(AssemblyContactAnalyzer{&refreshed_context}.analyze(project));
  CHECK(refreshed_contacts.component_occurrence_count == contacts.component_occurrence_count);
  CHECK(refreshed_contacts.recomputed_part_count == contacts.recomputed_part_count);
  REQUIRE(refreshed_contacts.records.size() == contacts.records.size());
  for (std::size_t index = 0; index < contacts.records.size(); ++index) {
    const auto& before = contacts.records[index];
    const auto& after = refreshed_contacts.records[index];
    CHECK(after.pair == before.pair);
    CHECK(after.classification == before.classification);
    CHECK(after.overlap_volume_mm3 == Approx(before.overlap_volume_mm3).margin(1e-5));
    REQUIRE(after.minimum_distance_mm.has_value() == before.minimum_distance_mm.has_value());
    if (before.minimum_distance_mm)
      CHECK(*after.minimum_distance_mm == Approx(*before.minimum_distance_mm).margin(1e-5));
  }
  const auto refreshed_json = json(project);
  const auto loaded = value(deserialize_project_from_json(refreshed_json));
  CHECK(json(loaded) == refreshed_json);
  CHECK(file_bytes(asset) == revised_bytes);

  StepImportTopologyContext export_context(fixture.root);
  const auto flat = value(AssemblyStepExporter{&export_context}.write_step(
      loaded, (fixture.root / "reference-flat.step").string()));
  CHECK(flat.recomputed_part_count == 1);
  CHECK(flat.exported_component_count == 4);
  const auto structured = value(AssemblyStructuredStepExporter{&export_context}.write_step(
      loaded, (fixture.root / "reference-structured.step").string()));
  CHECK(structured.part_product_definition_count == 1);
  CHECK(structured.exported_component_occurrence_count == 4);
  CHECK(structured.exported_assembly_occurrence_count == 5);
  same_physical_solids(exported_solids(fixture.root, "reference-flat.step"), expected);
  same_physical_solids(exported_solids(fixture.root, "reference-structured.step"), expected);
  CHECK(json(loaded) == refreshed_json);
  CHECK(file_bytes(asset) == revised_bytes);

  const auto reference_json = value(serialize_part_document_to_json(*reference));
  auto converted =
      value(commands.convert(loaded, fixture.part.id(), DocumentId("part.converted"),
                             "Editable copy", FeatureId("feature.converted"), fixture.root));
  CHECK(json(loaded) == refreshed_json);
  REQUIRE(converted.project.part_document_count() == 2);
  const auto* preserved_reference = converted.project.find_part_document(fixture.part.id());
  REQUIRE(preserved_reference);
  CHECK(value(serialize_part_document_to_json(*preserved_reference)) == reference_json);
  CHECK(value(AssemblyLeafOccurrenceResolver{}.resolve(converted.project)) == moved_leaves);
  const auto* editable = converted.project.find_part_document(converted.part);
  REQUIRE(editable);
  CHECK_FALSE(editable->is_reference_part());
  REQUIRE(editable->imported_body_features().size() == 1);
  const auto& base = editable->imported_body_features().front();
  CHECK(base.id() == FeatureId("feature.converted"));
  REQUIRE(base.definition().outputs().size() == 1);
  CHECK(base.definition().outputs().front().imported_body == output.imported_body);
  CHECK(base.definition().outputs().front().body != output.body);
  CHECK(editable->imported_topology_catalogs() == reference->imported_topology_catalogs());
  auto converted_cache = value(ShapeCache::create(ShapeCacheId("mvp.converted")));
  StepImportTopologyContext converted_context(fixture.root);
  REQUIRE(
      GeometryRecomputeExecutor{&converted_context}.execute_document(*editable, converted_cache));
  CHECK(converted_cache.reference_result() == nullptr);
  CHECK(converted_cache.feature_shape_count() == 1);
  REQUIRE(converted_cache.find_feature_shape(base.id()));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*converted_cache.find_feature_shape(base.id()))
            .volume_mm3 == Approx(1000));
  const auto converted_json = json(converted.project);
  CHECK(json(value(deserialize_project_from_json(converted_json))) == converted_json);
  check_persistent_identity_fields(nlohmann::json::parse(converted_json));
  CHECK(file_bytes(asset) == revised_bytes);
}

TEST_CASE("STEP Reference acceptance rejects unavailable and unresolved revisions without "
          "publishing model cache or export changes",
          "[geometry][integration][step-import-mvp][assembly-step-reference-part]") {
  const int failure = GENERATE(0, 1, 2, 3);
  CAPTURE(failure);
  Fixture fixture;
  const auto asset = fixture.root / "supplier.step";
  const auto supplier_bytes = file_bytes(asset);
  const StepReferenceCommands commands;
  const auto project = command_reference_project(fixture);
  const auto original_json = json(project);
  const auto* reference = project.find_part_document(fixture.part.id());
  REQUIRE(reference);
  const auto output = reference->reference_part_definition()->outputs().front();
  auto shapes = value(ShapeCache::create(ShapeCacheId("mvp.atomic")));
  StepImportTopologyContext original_context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&original_context}.execute_document(*reference, shapes));
  const auto* original_shape = OcctShapeView::get(*shapes.find_body_shape(output.body));
  REQUIRE(original_shape);
  const TopoDS_Shape original_topology = *original_shape;
  const auto original_physical = physical_solids({*shapes.find_body_shape(output.body)});
  auto proposed = value(AssemblyJointMotionSolver{&original_context}.move(
      project, AssemblyJointId("joint.local"), angle(25)));
  REQUIRE(proposed.converged());
  const auto flat_path = fixture.root / "accepted-flat.step";
  const auto structured_path = fixture.root / "accepted-structured.step";
  REQUIRE(AssemblyStepExporter{&original_context}.write_step(project, flat_path.string()));
  REQUIRE(AssemblyStructuredStepExporter{&original_context}.write_step(project,
                                                                       structured_path.string()));
  const auto prior_flat = file_bytes(flat_path), prior_structured = file_bytes(structured_path);
  auto attempted = project;
  if (failure == 0) {
    std::filesystem::rename(asset, fixture.root / "supplier-held.step");
    CHECK(value(inspect_step_import_source(fixture.source, fixture.root)).state() ==
          StepImportFreshness::Missing);
  } else if (failure == 1) {
    fixture.change_source();
    const auto stale_refresh = value(inspect_step_import_source(fixture.source, fixture.root));
    REQUIRE(stale_refresh.state() == StepImportFreshness::Changed);
    fixture.change_source();
    CHECK_FALSE(refresh_step_import_source(attempted, fixture.root, stale_refresh));
    CHECK(json(attempted) == original_json);
  } else {
    if (failure == 2)
      write_supplier_shape(asset, BRepPrimAPI_MakeBox(gp_Pnt(100, 200, 300), 2, 3, 4).Shape());
    else {
      TopoDS_Compound duplicate;
      BRep_Builder builder;
      builder.MakeCompound(duplicate);
      builder.Add(duplicate, original_topology);
      builder.Add(duplicate, BRepBuilderAPI_Copy(original_topology).Shape());
      write_supplier_shape(asset, duplicate);
    }
    const auto report = value(inspect_step_import_source(fixture.source, fixture.root));
    REQUIRE(report.observed_sha256_digest());
    REQUIRE(report.state() == StepImportFreshness::Changed);
    // Accept into a disposable Core copy to prove semantic Lost/Ambiguous
    // rejection with the current digest, independently of the stale-byte gate.
    REQUIRE(attempted.refresh_step_import_source(fixture.source, *report.observed_sha256_digest()));
    auto* owner = attempted.find_part_document(fixture.part.id());
    REQUIRE(owner);
    REQUIRE(owner->refresh_step_import_source(fixture.source, *report.observed_sha256_digest()));
    StepImportTopologyContext recovery_context(fixture.root);
    const auto* recovery = value(recovery_context.recover(*owner, fixture.catalog.id()));
    REQUIRE(recovery->find(output.imported_body));
    CHECK(recovery->find(output.imported_body)->status ==
          (failure == 2 ? ImportedTopologyRecoveryStatus::Lost
                        : ImportedTopologyRecoveryStatus::Ambiguous));
  }
  const auto attempted_json = json(attempted);
  const auto external_bytes =
      failure == 0 ? file_bytes(fixture.root / "supplier-held.step") : file_bytes(asset);
  const auto* attempted_part = attempted.find_part_document(fixture.part.id());
  REQUIRE(attempted_part);
  StepImportTopologyContext cache_context(fixture.root);
  CHECK_FALSE(GeometryRecomputeExecutor{&cache_context}.execute_document(*attempted_part, shapes));
  REQUIRE(shapes.find_body_shape(output.body));
  const auto* unchanged_shape = OcctShapeView::get(*shapes.find_body_shape(output.body));
  REQUIRE(unchanged_shape);
  CHECK(unchanged_shape->IsEqual(original_topology));
  same_physical_solids(physical_solids({*shapes.find_body_shape(output.body)}), original_physical);
  REQUIRE(shapes.reference_result());
  CHECK(shapes.reference_result()->source == fixture.source);
  CHECK(shapes.reference_result()->definition == *reference->reference_part_definition());
  CHECK(shapes.feature_shape_count() == 0);
  CHECK(shapes.body_shape_count() == 0);
  REQUIRE(shapes.reference_result()->bodies.size() == 1);
  CHECK(shapes.find_body_shape(output.body) == &shapes.reference_result()->bodies.front().shape);

  StepImportTopologyContext apply_context(fixture.root);
  CHECK_FALSE(AssemblyJointMotionResultApplier{&apply_context}.apply(attempted, proposed));
  CHECK_FALSE(commands.repeat(attempted, fixture.part.id(), attempted.assembly().id(),
                              ComponentInstanceId("rejected"), identity_rigid_transform(),
                              fixture.root));
  CHECK_FALSE(commands.convert(attempted, fixture.part.id(), DocumentId("rejected.copy"),
                               "Rejected copy", FeatureId("rejected.base"), fixture.root));
  if (failure != 1)
    CHECK_FALSE(commands.refresh(project, fixture.part.id(), fixture.root));
  StepImportTopologyContext analysis_context(fixture.root);
  CHECK_FALSE(AssemblyContactAnalyzer{&analysis_context}.analyze(attempted));
  CHECK_FALSE(ViewportSceneBuilder{&analysis_context}.build_project(attempted));
  StepImportTopologyContext export_context(fixture.root);
  CHECK_FALSE(AssemblyStepExporter{&export_context}.write_step(attempted, flat_path.string()));
  CHECK_FALSE(AssemblyStructuredStepExporter{&export_context}.write_step(attempted,
                                                                         structured_path.string()));
  CHECK(file_bytes(flat_path) == prior_flat);
  CHECK(file_bytes(structured_path) == prior_structured);
  CHECK(json(project) == original_json);
  CHECK(json(attempted) == attempted_json);
  if (failure == 0) {
    CHECK_FALSE(std::filesystem::exists(asset));
    CHECK(file_bytes(fixture.root / "supplier-held.step") == supplier_bytes);
  } else
    CHECK(file_bytes(asset) == external_bytes);
}
