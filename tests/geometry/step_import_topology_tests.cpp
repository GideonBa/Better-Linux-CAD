#include "blcad/core/imported_topology.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/step_import_topology.hpp"

#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Builder.hxx>
#include <Geom_BezierSurface.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <STEPCAFControl_Controller.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <TColgp_Array2OfPnt.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopoDS_Compound.hxx>
#include <UnitsMethods_LengthUnit.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Pln.hxx>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <unistd.h>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

namespace {

struct Fixture {
  std::filesystem::path root;
  Fixture() {
    static std::atomic<unsigned> next{};
    root = std::filesystem::temp_directory_path() /
           ("blcad-import-topology-" + std::to_string(::getpid()) + '-' + std::to_string(++next));
    REQUIRE(std::filesystem::create_directories(root));
  }
  ~Fixture() {
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }
  StepImportGeometry read(const TopoDS_Shape& shape, const char* unit = "MM",
                          const char* name = "Supplier product") const {
    Handle(TDocStd_Document) document = new TDocStd_Document("BinXCAF");
    XCAFDoc_DocumentTool::Set(document->Main());
    XCAFDoc_DocumentTool::SetLengthUnit(document, 1, UnitsMethods_LengthUnit_Millimeter);
    auto tool = XCAFDoc_DocumentTool::ShapeTool(document->Main());
    auto label = tool->AddShape(shape, false, false);
    TDataStd_Name::Set(label, TCollection_ExtendedString(name, true));
    STEPCAFControl_Controller::Init();
    const std::string old_unit = Interface_Static::CVal("write.step.unit");
    REQUIRE(Interface_Static::SetCVal("write.step.unit", unit));
    STEPCAFControl_Writer writer;
    writer.SetNameMode(true);
    const bool transferred = writer.Transfer(document, STEPControl_AsIs);
    Interface_Static::SetCVal("write.step.unit", old_unit.c_str());
    REQUIRE(transferred);
    const auto path = root / "asset.step";
    REQUIRE(writer.Write(path.c_str()) == IFSelect_RetDone);
    std::ifstream stream(path, std::ios::binary);
    REQUIRE(stream.good());
    const std::string bytes(std::istreambuf_iterator<char>(stream), {});
    auto digest = step_import_sha256(bytes);
    REQUIRE(digest);
    auto source = StepImportSource::create(StepImportSourceId("source.topology"), name,
                                           "asset.step", digest.value(), StepImportMode::Reference);
    REQUIRE(source);
    auto imported = StepImportReader{}.read(source.value(), root);
    INFO((imported ? "read" : imported.error().message()));
    REQUIRE(imported);
    return imported.value();
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

ImportedTopologyCatalog capture(const StepImportGeometry& imported) {
  REQUIRE(imported.products.size() == 1);
  auto result = StepImportTopology{}.create_catalog(ImportedTopologyCatalogId("catalog.supplier"),
                                                    imported, 0);
  INFO((result ? "captured" : result.error().message()));
  REQUIRE(result);
  return result.value();
}

template <typename Predicate>
const ImportedTopologyEntry& find_entry(const ImportedTopologyCatalog& catalog,
                                        Predicate predicate) {
  const auto found = std::find_if(catalog.entries().begin(), catalog.entries().end(), predicate);
  REQUIRE(found != catalog.entries().end());
  return *found;
}

const ImportedTopologyEntry& bottom(const ImportedTopologyCatalog& catalog) {
  return find_entry(catalog, [](const auto& entry) {
    return entry.descriptor.kind == ImportedTopologyKind::Face &&
           std::abs(entry.descriptor.centroid.z) < 1e-6;
  });
}

ImportedTopologyReference reference(const ImportedTopologyCatalog& catalog,
                                    const ImportedTopologyEntry& entry,
                                    ImportedTopologyCapability capability) {
  auto result = ImportedTopologyReference::create(catalog.id(), entry.id, capability);
  REQUIRE(result);
  return result.value();
}

ImportedTopologyRecovery recover(const ImportedTopologyCatalog& catalog,
                                 const StepImportGeometry& imported) {
  auto result = StepImportTopology{}.recover(catalog, imported);
  INFO((result ? "recovered" : result.error().message()));
  REQUIRE(result);
  return result.value();
}

} // namespace

TEST_CASE("Imported topology captures stable body face edge and vertex ids with exact capabilities",
          "[geometry][step-import-topology-recovery]") {
  Fixture fixture;
  auto imported = fixture.read(box());
  const auto catalog = capture(imported);
  REQUIRE(catalog.entries().size() == 27);
  CHECK(catalog.source() == imported.source.id());
  CHECK(catalog.captured_sha256_digest() == imported.source.sha256_digest());
  CHECK(catalog.product_key() != imported.products.front().definition_key);
  const auto current = recover(catalog, imported);
  REQUIRE(current.entries.size() == catalog.entries().size());
  std::array<std::size_t, 4> counts{};
  for (const auto& saved : catalog.entries()) {
    const auto* entry = current.find(saved.id);
    REQUIRE(entry != nullptr);
    REQUIRE(entry->status == ImportedTopologyRecoveryStatus::Resolved);
    CHECK(entry->candidate_count == 1);
    CHECK_FALSE(entry->shape.empty());
    REQUIRE(entry->descriptor);
    CHECK(entry->capabilities == imported_topology_capabilities(*entry->descriptor));
    ++counts[static_cast<std::size_t>(saved.descriptor.kind)];
    if (saved.descriptor.kind == ImportedTopologyKind::Face) {
      CHECK(entry->descriptor->geometry == ImportedGeometryClass::Plane);
      CHECK_FALSE(entry->descriptor->boundary_keys.empty());
      auto projected = StepImportTopology{}.resolve(
          current, reference(catalog, saved, ImportedTopologyCapability::Plane));
      REQUIRE(projected);
      CHECK(length(projected.value().descriptor->direction) == Approx(1));
    }
    if (saved.descriptor.kind == ImportedTopologyKind::Edge)
      CHECK(StepImportTopology{}.resolve(
          current, reference(catalog, saved, ImportedTopologyCapability::Line)));
    if (saved.descriptor.kind == ImportedTopologyKind::Vertex)
      CHECK(StepImportTopology{}.resolve(
          current, reference(catalog, saved, ImportedTopologyCapability::Point)));
  }
  CHECK((counts == std::array<std::size_t, 4>{1, 6, 12, 8}));
  const auto& face = bottom(catalog);
  CHECK_FALSE(StepImportTopology{}.resolve(
      current, reference(catalog, face, ImportedTopologyCapability::Cylinder)));
}

TEST_CASE("Imported topology physical witnesses survive units names and body insertion reordering",
          "[geometry][step-import-topology-recovery]") {
  const auto* unit = GENERATE("MM", "INCH", "M");
  CAPTURE(unit);
  Fixture fixture;
  auto first =
      fixture.read(compound({box(), BRepPrimAPI_MakeBox(gp_Pnt(50, 0, 0), 7, 9, 11).Shape()}));
  const auto catalog = capture(first);
  auto changed =
      fixture.read(compound({BRepPrimAPI_MakeBox(gp_Pnt(50, 0, 0), 7, 9, 11).Shape(), box()}), unit,
                   "Renamed product without identity authority");
  const auto rebuilt = capture(changed);
  CHECK(rebuilt.product_key() == catalog.product_key());
  REQUIRE(rebuilt.entries().size() == catalog.entries().size());
  for (std::size_t i = 0; i < catalog.entries().size(); ++i) {
    CHECK(rebuilt.entries()[i].id == catalog.entries()[i].id);
    CHECK(rebuilt.entries()[i].parent_body == catalog.entries()[i].parent_body);
    CHECK(rebuilt.entries()[i].descriptor.geometry_key ==
          catalog.entries()[i].descriptor.geometry_key);
  }
  const auto current = recover(catalog, changed);
  for (const auto& entry : current.entries)
    CHECK(entry.status == ImportedTopologyRecoveryStatus::Resolved);
}

TEST_CASE("Imported topology preserves unchanged trimmed faces after another face changes",
          "[geometry][step-import-topology-recovery]") {
  Fixture fixture;
  const auto catalog = capture(fixture.read(box(30)));
  const auto original_entries = catalog.entries();
  const auto current = recover(catalog, fixture.read(box(40)));
  const auto& base = bottom(catalog);
  const auto* base_resolution = current.find(base.id);
  REQUIRE(base_resolution);
  CHECK(base_resolution->status == ImportedTopologyRecoveryStatus::Resolved);
  auto projected = StepImportTopology{}.resolve(
      current, reference(catalog, base, ImportedTopologyCapability::Plane));
  REQUIRE(projected);
  CHECK(projected.value().descriptor->origin.z == Approx(0));
  const auto& top = find_entry(catalog, [](const auto& entry) {
    return entry.descriptor.kind == ImportedTopologyKind::Face &&
           std::abs(entry.descriptor.centroid.z - 30) < 1e-6;
  });
  REQUIRE(current.find(top.id));
  CHECK(current.find(top.id)->status == ImportedTopologyRecoveryStatus::Lost);
  const auto& body = find_entry(catalog, [](const auto& entry) {
    return entry.descriptor.kind == ImportedTopologyKind::Body;
  });
  REQUIRE(current.find(body.id));
  CHECK(current.find(body.id)->status == ImportedTopologyRecoveryStatus::Resolved);
  CHECK(catalog.entries() == original_entries);
  CHECK(current.current_sha256_digest != catalog.captured_sha256_digest());
}

TEST_CASE("Imported topology never chooses a nearest removed split or duplicate body",
          "[geometry][step-import-topology-recovery]") {
  Fixture fixture;
  const auto catalog = capture(fixture.read(box()));
  SECTION("removed body") {
    auto current =
        recover(catalog, fixture.read(BRepPrimAPI_MakeBox(gp_Pnt(100, 0, 0), 10, 20, 30).Shape()));
    for (const auto& entry : current.entries) {
      CHECK(entry.status == ImportedTopologyRecoveryStatus::Lost);
      CHECK(entry.shape.empty());
      CHECK_FALSE(entry.descriptor);
    }
  }
  SECTION("equal duplicate bodies") {
    auto current = recover(catalog, fixture.read(compound({box(), box()})));
    for (const auto& entry : current.entries) {
      CHECK(entry.status == ImportedTopologyRecoveryStatus::Ambiguous);
      CHECK(entry.shape.empty());
      CHECK_FALSE(entry.descriptor);
    }
  }
  SECTION("surviving trimmed faces disagree about their current enclosing solid") {
    auto lower = BRepPrimAPI_MakeBox(10, 20, 20).Shape();
    auto upper = BRepPrimAPI_MakeBox(gp_Pnt(0, 0, 10), 10, 20, 20).Shape();
    auto current = recover(catalog, fixture.read(compound({lower, upper})));
    for (const auto& entry : current.entries) {
      CHECK(entry.status == ImportedTopologyRecoveryStatus::Ambiguous);
      CHECK(entry.shape.empty());
    }
  }
  SECTION("split surface boundary") {
    auto whole =
        BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 0, 10, 0, 20).Shape();
    const auto surface_catalog = capture(fixture.read(whole));
    auto first =
        BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 0, 5, 0, 20).Shape();
    auto second =
        BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 5, 10, 0, 20).Shape();
    auto current = recover(surface_catalog, fixture.read(compound({first, second})));
    for (const auto& entry : current.entries)
      CHECK(entry.status == ImportedTopologyRecoveryStatus::Lost);
  }
}

TEST_CASE("Imported cylinders circles and vertices expose only exact analytic projections",
          "[geometry][step-import-topology-recovery]") {
  Fixture fixture;
  const auto catalog = capture(fixture.read(BRepPrimAPI_MakeCylinder(5, 20).Shape()));
  const auto current =
      recover(catalog, fixture.read(BRepPrimAPI_MakeCylinder(5, 20).Shape(), "INCH"));
  const auto& cylinder = find_entry(catalog, [](const auto& entry) {
    return entry.descriptor.geometry == ImportedGeometryClass::Cylinder;
  });
  const auto wall = StepImportTopology{}.resolve(
      current, reference(catalog, cylinder, ImportedTopologyCapability::Cylinder));
  REQUIRE(wall);
  CHECK(wall.value().descriptor->radius_mm == Approx(5));
  CHECK(wall.value().descriptor->direction.z == Approx(1));
  CHECK(StepImportTopology{}.resolve(
      current, reference(catalog, cylinder, ImportedTopologyCapability::Axis)));
  CHECK_FALSE(StepImportTopology{}.resolve(
      current, reference(catalog, cylinder, ImportedTopologyCapability::Plane)));
  const auto& circle = find_entry(catalog, [](const auto& entry) {
    return entry.descriptor.geometry == ImportedGeometryClass::Circle;
  });
  for (const auto capability :
       {ImportedTopologyCapability::Circle, ImportedTopologyCapability::Axis,
        ImportedTopologyCapability::Point})
    CHECK(StepImportTopology{}.resolve(current, reference(catalog, circle, capability)));
}

TEST_CASE("Imported opaque surface and curve equations never masquerade as analytic primitives",
          "[geometry][step-import-topology-recovery]") {
  TColgp_Array2OfPnt poles(1, 3, 1, 3);
  for (int u = 1; u <= 3; ++u)
    for (int v = 1; v <= 3; ++v)
      poles.SetValue(u, v, gp_Pnt((u - 1) * 10, (v - 1) * 10, u == 2 && v == 2 ? 8 : 0));
  Handle(Geom_BezierSurface) surface = new Geom_BezierSurface(poles);
  Fixture fixture;
  const auto imported = fixture.read(BRepBuilderAPI_MakeFace(surface, 1e-7).Shape());
  const auto catalog = capture(imported);
  const auto current = recover(catalog, imported);
  const auto& face = find_entry(catalog, [](const auto& entry) {
    return entry.descriptor.kind == ImportedTopologyKind::Face;
  });
  CHECK(face.descriptor.geometry == ImportedGeometryClass::Opaque);
  CHECK(StepImportTopology{}.resolve(current,
                                     reference(catalog, face, ImportedTopologyCapability::Face)));
  CHECK_FALSE(StepImportTopology{}.resolve(
      current, reference(catalog, face, ImportedTopologyCapability::Plane)));
  CHECK_FALSE(StepImportTopology{}.resolve(
      current, reference(catalog, face, ImportedTopologyCapability::Cylinder)));
}

TEST_CASE("Imported topology downstream matching requires complete bounded face and edge evidence",
          "[geometry][step-import-topology-recovery]") {
  Fixture fixture;
  const auto imported = fixture.read(box());
  const auto catalog = capture(imported);
  const auto current = recover(catalog, imported);
  const auto* resolved = current.find(bottom(catalog).id);
  REQUIRE(resolved);
  REQUIRE(resolved->descriptor);
  const auto* target = OcctShapeView::get(imported.products.front().bodies.front().shape);
  REQUIRE(target);
  auto exact = StepImportTopology{}.match_subshape(*resolved->descriptor, *target);
  REQUIRE(exact);
  CHECK(exact.value().status == ImportedTopologyRecoveryStatus::Resolved);
  auto larger = BRepPrimAPI_MakeBox(10, 25, 30).Shape();
  auto other = StepImportTopology{}.match_subshape(*resolved->descriptor, larger);
  REQUIRE(other);
  CHECK(other.value().status == ImportedTopologyRecoveryStatus::Lost);
  const auto* face_shape = OcctShapeView::get(resolved->shape);
  REQUIRE(face_shape);
  auto duplicate = compound({*face_shape, BRepBuilderAPI_Copy(*face_shape).Shape()});
  auto ambiguous = StepImportTopology{}.match_subshape(*resolved->descriptor, duplicate);
  REQUIRE(ambiguous);
  CHECK(ambiguous.value().status == ImportedTopologyRecoveryStatus::Ambiguous);
  CHECK(ambiguous.value().candidate_count == 2);
  const auto& edge = find_entry(catalog, [](const auto& entry) {
    return entry.descriptor.kind == ImportedTopologyKind::Edge;
  });
  auto same_edge = StepImportTopology{}.match_subshape(edge.descriptor, *target);
  REQUIRE(same_edge);
  CHECK(same_edge.value().status == ImportedTopologyRecoveryStatus::Resolved);
}

TEST_CASE("Imported topology rejects wrong source catalog and capability identity",
          "[geometry][step-import-topology-recovery]") {
  Fixture fixture;
  auto imported = fixture.read(box());
  const auto catalog = capture(imported);
  SECTION("source identity") {
    auto wrong =
        StepImportSource::create(StepImportSourceId("source.other"), "Other", "asset.step",
                                 imported.source.sha256_digest(), StepImportMode::Reference);
    REQUIRE(wrong);
    imported.source = wrong.value();
    CHECK_FALSE(StepImportTopology{}.recover(catalog, imported));
  }
  SECTION("catalog identity") {
    const auto current = recover(catalog, imported);
    auto wrong =
        ImportedTopologyReference::create(ImportedTopologyCatalogId("other.catalog"),
                                          bottom(catalog).id, ImportedTopologyCapability::Plane);
    REQUIRE(wrong);
    CHECK_FALSE(StepImportTopology{}.resolve(current, wrong.value()));
  }
  SECTION("missing entry") {
    const auto current = recover(catalog, imported);
    auto wrong = ImportedTopologyReference::create(
        catalog.id(), ImportedTopologyId("missing.entry"), ImportedTopologyCapability::Plane);
    REQUIRE(wrong);
    CHECK_FALSE(StepImportTopology{}.resolve(current, wrong.value()));
  }
  SECTION("unavailable product") {
    CHECK_FALSE(StepImportTopology{}.create_catalog(catalog.id(), imported, 99));
  }
}
