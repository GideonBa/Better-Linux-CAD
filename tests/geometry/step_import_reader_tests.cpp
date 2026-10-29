#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/step_import_reader.hpp"

#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Builder.hxx>
#include <Bnd_Box.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <STEPCAFControl_Controller.hxx>
#include <STEPConstruct_ExternRefs.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <StepBasic_ProductDefinition.hxx>
#include <StepData_StepModel.hxx>
#include <StepData_GlobalFactors.hxx>
#include <StepGeom_CartesianPoint.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shell.hxx>
#include <UnitsMethods_LengthUnit.hxx>
#include <UnitsMethods.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XSAlgo.hxx>
#include <XSAlgo_AlgoContainer.hxx>
#include <XSControl_WorkSession.hxx>
#include <gp_Ax1.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <regex>
#include <string>
#include <tuple>
#include <unistd.h>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

namespace {

struct Fixture {
  std::filesystem::path root;
  Handle(TDocStd_Document) document;
  Handle(XCAFDoc_ShapeTool) tool;

  Fixture() {
    static std::atomic<unsigned> ordinal{0};
    root = std::filesystem::temp_directory_path() /
           ("blcad-step-reader-" + std::to_string(::getpid()) + '-' + std::to_string(++ordinal));
    REQUIRE(std::filesystem::create_directories(root));
    document = new TDocStd_Document("BinXCAF");
    XCAFDoc_DocumentTool::Set(document->Main());
    XCAFDoc_DocumentTool::SetLengthUnit(document, 1, UnitsMethods_LengthUnit_Millimeter);
    tool = XCAFDoc_DocumentTool::ShapeTool(document->Main());
    REQUIRE_FALSE(tool.IsNull());
  }
  ~Fixture() {
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }
  TDF_Label part(const TopoDS_Shape& shape, const char* name) {
    auto label = tool->AddShape(shape, false, false);
    REQUIRE_FALSE(label.IsNull());
    TDataStd_Name::Set(label, TCollection_ExtendedString(name, true));
    return label;
  }
  TDF_Label assembly(const char* name) {
    auto label = tool->NewShape();
    TDataStd_Name::Set(label, TCollection_ExtendedString(name, true));
    return label;
  }
  void component(const TDF_Label& parent, const TDF_Label& child, double x,
                 const char* name = "Occurrence", bool rotate = false) {
    gp_Trsf placement;
    if (rotate)
      placement.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 1.57079632679489661923);
    placement.SetTranslationPart(gp_Vec(x, 0, 0));
    auto label = tool->AddComponent(parent, child, TopLoc_Location(placement));
    REQUIRE_FALSE(label.IsNull());
    TDataStd_Name::Set(label, TCollection_ExtendedString(name, true));
  }
  std::filesystem::path write(const char* name = "asset.step", const char* unit = "MM",
                              bool external = false) {
    tool->UpdateAssemblies();
    STEPCAFControl_Controller::Init();
    const std::string old_unit = Interface_Static::CVal("write.step.unit");
    const std::string old_schema = Interface_Static::CVal("write.step.schema");
    REQUIRE(Interface_Static::SetCVal("write.step.unit", unit));
    REQUIRE(Interface_Static::SetCVal("write.step.schema", "AP214IS"));
    STEPCAFControl_Writer writer;
    const bool transferred = writer.Transfer(document, STEPControl_AsIs);
    Interface_Static::SetCVal("write.step.unit", old_unit.c_str());
    Interface_Static::SetCVal("write.step.schema", old_schema.c_str());
    REQUIRE(transferred);
    if (external) {
      auto model = writer.ChangeWriter().Model();
      Handle(StepBasic_ProductDefinition) product;
      for (int i = 1; i <= model->NbEntities() && product.IsNull(); ++i)
        product = Handle(StepBasic_ProductDefinition)::DownCast(model->Value(i));
      REQUIRE_FALSE(product.IsNull());
      STEPConstruct_ExternRefs references(writer.ChangeWriter().WS());
      REQUIRE(references.AddExternRef("../../outside.step", product, "STEP AP214") > 0);
      REQUIRE(references.WriteExternRefs(1) > 0);
    }
    auto path = root / name;
    REQUIRE(writer.Write(path.c_str()) == IFSelect_RetDone);
    REQUIRE(std::filesystem::file_size(path) > 0);
    return path;
  }
  std::string bytes(const std::filesystem::path& path) const {
    std::ifstream stream(path, std::ios::binary);
    REQUIRE(stream.good());
    return std::string(std::istreambuf_iterator<char>(stream), {});
  }
  void replace(const std::filesystem::path& path, const std::string& data) const {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << data;
    REQUIRE(stream.good());
  }
  StepImportSource source(const std::filesystem::path& path) const {
    const auto hash = step_import_sha256(bytes(path));
    REQUIRE(hash);
    auto result =
        StepImportSource::create(StepImportSourceId("source.reader"), "Supplier model",
                                 path.filename().string(), hash.value(), StepImportMode::Reference);
    REQUIRE(result);
    return result.value();
  }
};

std::array<double, 6> bounds(const GeometryShape& geometry) {
  const auto* shape = OcctShapeView::get(geometry);
  REQUIRE(shape != nullptr);
  Bnd_Box box;
  BRepBndLib::AddOptimal(*shape, box, false, false);
  std::array<double, 6> result{};
  box.Get(result[0], result[1], result[2], result[3], result[4], result[5]);
  return result;
}

const StepImportedBodyDefinition& only_body(const StepImportGeometry& imported) {
  REQUIRE(imported.products.size() == 1);
  REQUIRE(imported.products.front().bodies.size() == 1);
  return imported.products.front().bodies.front();
}

std::string presentation_signature(const StepImportGeometry& imported) {
  std::string value;
  for (const auto& product : imported.products) {
    value += product.definition_key + ':' + product.name + ';';
    for (const auto& body : product.bodies)
      value += body.definition_key + ':' + body.name + ';';
  }
  for (const auto& occurrence : imported.occurrences) {
    value += occurrence.parent_definition ? std::to_string(*occurrence.parent_definition) : "root";
    value += ':' + std::to_string(occurrence.product_definition) + ':' + occurrence.name;
    for (double entry : occurrence.transform.matrix)
      value += ',' + std::to_string(entry);
  }
  return value;
}

} // namespace

TEST_CASE("STEP reader normalizes declared mm inch and metre geometry without source mutation",
          "[geometry][step-import-reader]") {
  struct UnitCase {
    const char* name;
    double scale;
  };
  const auto unit = GENERATE(UnitCase{"MM", 1}, UnitCase{"INCH", 25.4}, UnitCase{"M", 1000});
  CAPTURE(unit.name);
  Fixture fixture;
  fixture.part(BRepPrimAPI_MakeBox(25.4, 50.8, 76.2).Shape(), "Supplier plate Ä");
  const auto path = fixture.write("unit.step", unit.name);
  const auto original = fixture.bytes(path);
  // Inspect the actual written coordinates independently of geometry transfer. Writer models
  // snapshot output units at construction; mislabeled fixtures must never change the physical
  // dimension expectations of the reader test.
  STEPControl_Reader raw;
  REQUIRE(raw.ReadFile(path.c_str()) == IFSelect_RetDone);
  std::array<double, 3> maximum{};
  for (int index = 1; index <= raw.StepModel()->NbEntities(); ++index) {
    auto point = Handle(StepGeom_CartesianPoint)::DownCast(raw.StepModel()->Value(index));
    if (!point.IsNull() && point->NbCoordinates() == 3)
      for (std::size_t coordinate = 0; coordinate < maximum.size(); ++coordinate)
        maximum[coordinate] = std::max(maximum[coordinate],
                                       point->CoordinatesValue(static_cast<int>(coordinate + 1)));
  }
  CHECK(maximum[0] * unit.scale == Approx(25.4));
  CHECK(maximum[1] * unit.scale == Approx(50.8));
  CHECK(maximum[2] * unit.scale == Approx(76.2));
  auto imported = StepImportReader{}.read(fixture.source(path), fixture.root);
  INFO((imported ? "imported" : imported.error().message()));
  REQUIRE(imported);
  REQUIRE(imported.value().units.size() == 1);
  CHECK(imported.value().units.front().length_to_mm == Approx(unit.scale));
  const auto& body = only_body(imported.value());
  CHECK(body.kind == BodyKind::Solid);
  CHECK(body.source_kind == StepImportedShapeKind::Solid);
  CHECK(body.bounds_mm[3] == Approx(25.4));
  CHECK(body.bounds_mm[4] == Approx(50.8));
  CHECK(body.bounds_mm[5] == Approx(76.2));
  CHECK(body.volume_mm3 == Approx(25.4 * 50.8 * 76.2));
  CHECK(body.face_count == 6);
  CHECK(imported.value().products.front().name == "Supplier plate Ä");
  CHECK(imported.value().products.front().source_kind == StepImportedShapeKind::Solid);
  CHECK_FALSE(body.definition_key.empty());
  CHECK(fixture.bytes(path) == original);
  CHECK(imported.value().diagnostics.back().find("disabled") != std::string::npos);
  auto preview = StepImportReader{}.preview_shapes(imported.value());
  REQUIRE(preview);
  REQUIRE(preview.value().size() == 1);
  CHECK(bounds(preview.value().front()) == body.bounds_mm);
}

TEST_CASE("STEP reader preserves shared nested definitions and composes direct placements once",
          "[geometry][step-import-reader]") {
  Fixture fixture;
  auto body = fixture.part(BRepPrimAPI_MakeBox(10, 20, 30).Shape(), "Shared part");
  auto child = fixture.assembly("Shared subassembly");
  fixture.component(child, body, 3, "Inner rotated component", true);
  auto root = fixture.assembly("Root product");
  fixture.component(root, child, -100, "Left");
  fixture.component(root, child, 100, "Right");
  const auto path = fixture.write();
  auto imported = StepImportReader{}.read(fixture.source(path), fixture.root);
  INFO((imported ? "imported" : imported.error().message()));
  REQUIRE(imported);
  CHECK(imported.value().products.size() == 3);
  CHECK(imported.value().body_count() == 1);
  CHECK(imported.value().occurrences.size() == 4);
  CHECK(std::count_if(imported.value().products.begin(), imported.value().products.end(),
                      [](const auto& p) { return p.is_assembly; }) == 2);
  auto preview = StepImportReader{}.preview_shapes(imported.value());
  REQUIRE(preview);
  REQUIRE(preview.value().size() == 2);
  auto first = bounds(preview.value()[0]);
  auto second = bounds(preview.value()[1]);
  if (first[0] > second[0])
    std::swap(first, second);
  CHECK(first[0] == Approx(-117));
  CHECK(first[3] == Approx(-97));
  CHECK(first[4] == Approx(10));
  CHECK(second[0] == Approx(83));
  CHECK(second[3] == Approx(103));
  CHECK(second[4] == Approx(10));
}

TEST_CASE("STEP reader ordering is independent of XDE insertion and body traversal order",
          "[geometry][step-import-reader]") {
  const auto create = [](Fixture& fixture, bool reverse) {
    const TopoDS_Shape small = BRepPrimAPI_MakeBox(10, 20, 30).Shape();
    const TopoDS_Shape large = BRepPrimAPI_MakeBox(11, 22, 33).Shape();
    TDF_Label a, b;
    if (reverse) {
      b = fixture.part(large, "Duplicate name");
      a = fixture.part(small, "Duplicate name");
    } else {
      a = fixture.part(small, "Duplicate name");
      b = fixture.part(large, "Duplicate name");
    }
    auto root = fixture.assembly("Root");
    if (reverse) {
      fixture.component(root, b, 50, "B");
      fixture.component(root, a, 5, "A");
    } else {
      fixture.component(root, a, 5, "A");
      fixture.component(root, b, 50, "B");
    }
    return fixture.write();
  };
  Fixture forward, reverse;
  auto p1 = create(forward, false);
  auto p2 = create(reverse, true);
  auto a = StepImportReader{}.read(forward.source(p1), forward.root);
  auto b = StepImportReader{}.read(reverse.source(p2), reverse.root);
  REQUIRE(a);
  REQUIRE(b);
  CHECK(presentation_signature(a.value()) == presentation_signature(b.value()));
  for (const auto* imported : {&a.value(), &b.value()}) {
    CHECK(std::count_if(imported->occurrences.begin(), imported->occurrences.end(),
                        [](const auto& occurrence) { return occurrence.name == "A"; }) == 1);
    CHECK(std::count_if(imported->occurrences.begin(), imported->occurrences.end(),
                        [](const auto& occurrence) { return occurrence.name == "B"; }) == 1);
  }

  Fixture compound_forward, compound_reverse;
  const auto compound = [](Fixture& fixture, bool reverse_order) {
    BRep_Builder builder;
    TopoDS_Compound value;
    builder.MakeCompound(value);
    auto small = BRepPrimAPI_MakeBox(10, 20, 30).Shape();
    auto large = BRepPrimAPI_MakeBox(gp_Pnt(40, 0, 0), 11, 22, 33).Shape();
    builder.Add(value, reverse_order ? large : small);
    builder.Add(value, reverse_order ? small : large);
    fixture.part(value, "Two bodies");
    return fixture.write();
  };
  auto c1 = compound(compound_forward, false);
  auto c2 = compound(compound_reverse, true);
  auto c = StepImportReader{}.read(compound_forward.source(c1), compound_forward.root);
  auto d = StepImportReader{}.read(compound_reverse.source(c2), compound_reverse.root);
  REQUIRE(c);
  REQUIRE(d);
  CHECK(c.value().body_count() == 2);
  CHECK(c.value().products.front().source_kind == StepImportedShapeKind::Compound);
  CHECK(presentation_signature(c.value()) == presentation_signature(d.value()));
}

TEST_CASE("STEP reader retains equal products and names as metadata with explicit ambiguity",
          "[geometry][step-import-reader]") {
  Fixture fixture;
  auto a = fixture.part(BRepPrimAPI_MakeBox(10, 20, 30).Shape(), "Equal name");
  auto b = fixture.part(BRepPrimAPI_MakeBox(10, 20, 30).Shape(), "Equal name");
  auto root = fixture.assembly("Root");
  fixture.component(root, a, 0, "First");
  fixture.component(root, b, 50, "Second");
  const auto path = fixture.write();
  auto imported = StepImportReader{}.read(fixture.source(path), fixture.root);
  REQUIRE(imported);
  CHECK(imported.value().products.size() == 3);
  CHECK(imported.value().body_count() == 2);
  CHECK(std::count_if(
            imported.value().products.begin(), imported.value().products.end(),
            [](const auto& p) { return p.source_kind == StepImportedShapeKind::Solid; }) == 2);
  CHECK(std::count_if(imported.value().products.begin(), imported.value().products.end(),
                      [](const auto& p) { return p.ambiguous_order; }) == 2);
  CHECK(std::any_of(
      imported.value().diagnostics.begin(), imported.value().diagnostics.end(),
      [](const auto& text) { return text.find("retain separate") != std::string::npos; }));
  auto preview = StepImportReader{}.preview_shapes(imported.value());
  REQUIRE(preview);
  CHECK(preview.value().size() == 2);

  Fixture renamed;
  renamed.part(BRepPrimAPI_MakeBox(10, 20, 30).Shape(), "Different metadata");
  const auto renamed_path = renamed.write();
  auto other = StepImportReader{}.read(renamed.source(renamed_path), renamed.root);
  REQUIRE(other);
  const auto& same_shape =
      *std::find_if(imported.value().products.begin(), imported.value().products.end(),
                    [](const auto& p) { return !p.is_assembly; });
  CHECK(same_shape.definition_key == other.value().products.front().definition_key);
}

TEST_CASE("STEP reader distinguishes compound solid and open surface results",
          "[geometry][step-import-reader]") {
  Fixture fixture;
  BRep_Builder builder;
  TopoDS_Compound compound;
  builder.MakeCompound(compound);
  builder.Add(compound, BRepPrimAPI_MakeBox(10, 20, 30).Shape());
  auto face =
      BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(40, 0, 0), gp_Dir(0, 0, 1)), 0, 10, 0, 20).Shape();
  TopoDS_Shell shell;
  builder.MakeShell(shell);
  builder.Add(shell, face);
  builder.Add(compound, shell);
  fixture.part(compound, "Solid and surface");
  const auto path = fixture.write();
  auto imported = StepImportReader{}.read(fixture.source(path), fixture.root);
  INFO((imported ? "imported" : imported.error().message()));
  REQUIRE(imported);
  CHECK(imported.value().products.size() == 1);
  CHECK(imported.value().body_count() == 2);
  CHECK(imported.value().products.front().source_kind == StepImportedShapeKind::Compound);
  const auto& bodies = imported.value().products.front().bodies;
  auto surface = std::find_if(bodies.begin(), bodies.end(),
                              [](const auto& body) { return body.kind == BodyKind::Surface; });
  REQUIRE(surface != bodies.end());
  CHECK(surface->source_kind == StepImportedShapeKind::Shell);
  CHECK(surface->area_mm2 == Approx(200));
  CHECK(surface->volume_mm3 == 0);
  CHECK(std::any_of(
      imported.value().diagnostics.begin(), imported.value().diagnostics.end(),
      [](const auto& message) { return message.find("Compound") != std::string::npos; }));
}

TEST_CASE("STEP reader does not apply a located root definition twice",
          "[geometry][step-import-reader]") {
  Fixture fixture;
  gp_Trsf placement;
  placement.SetTranslation(gp_Vec(25, -3, 4));
  fixture.part(BRepPrimAPI_MakeBox(10, 20, 30).Shape().Moved(TopLoc_Location(placement)),
               "Located root");
  const auto path = fixture.write();
  auto imported = StepImportReader{}.read(fixture.source(path), fixture.root);
  REQUIRE(imported);
  // OCCT writes a free located shape as an auxiliary assembly and one located component.
  REQUIRE(imported.value().products.size() == 2);
  CHECK(imported.value().body_count() == 1);
  CHECK(std::count_if(imported.value().products.begin(), imported.value().products.end(),
                      [](const auto& product) { return product.is_assembly; }) == 1);
  const auto product =
      std::find_if(imported.value().products.begin(), imported.value().products.end(),
                   [](const auto& value) { return !value.is_assembly; });
  REQUIRE(product != imported.value().products.end());
  REQUIRE(product->bodies.size() == 1);
  const auto& body = product->bodies.front();
  CHECK(body.bounds_mm[0] == Approx(0));
  CHECK(body.bounds_mm[3] == Approx(10));
  REQUIRE(imported.value().occurrences.size() == 2);
  const auto root_occurrence =
      std::find_if(imported.value().occurrences.begin(), imported.value().occurrences.end(),
                   [](const auto& occurrence) { return !occurrence.parent_definition; });
  const auto child_occurrence =
      std::find_if(imported.value().occurrences.begin(), imported.value().occurrences.end(),
                   [](const auto& occurrence) { return occurrence.parent_definition.has_value(); });
  REQUIRE(root_occurrence != imported.value().occurrences.end());
  REQUIRE(child_occurrence != imported.value().occurrences.end());
  CHECK(root_occurrence->transform.matrix[3] == Approx(0));
  CHECK(child_occurrence->transform.matrix[3] == Approx(25));
  auto preview = StepImportReader{}.preview_shapes(imported.value());
  REQUIRE(preview);
  const auto posed = bounds(preview.value().front());
  CHECK(posed[0] == Approx(25));
  CHECK(posed[1] == Approx(-3));
  CHECK(posed[2] == Approx(4));
  CHECK(posed[3] == Approx(35));
}

TEST_CASE("STEP reader preserves located geometry through nested noncommuting placements",
          "[geometry][step-import-reader]") {
  Fixture fixture;
  gp_Trsf local;
  local.SetTranslation(gp_Vec(25, -3, 4));
  auto body = fixture.part(BRepPrimAPI_MakeBox(10, 20, 30).Shape().Moved(TopLoc_Location(local)),
                           "Located part");
  auto child = fixture.assembly("Child");
  fixture.component(child, body, 3, "Rotate located definition", true);
  auto root = fixture.assembly("Root");
  fixture.component(root, child, 40, "Rotate child", true);
  fixture.tool->UpdateAssemblies();
  Bnd_Box oracle;
  BRepBndLib::AddOptimal(XCAFDoc_ShapeTool::GetShape(root), oracle, false, false);
  std::array<double, 6> expected{};
  oracle.Get(expected[0], expected[1], expected[2], expected[3], expected[4], expected[5]);
  const auto path = fixture.write();
  auto imported = StepImportReader{}.read(fixture.source(path), fixture.root);
  REQUIRE(imported);
  auto preview = StepImportReader{}.preview_shapes(imported.value());
  REQUIRE(preview);
  REQUIRE(preview.value().size() == 1);
  const auto actual = bounds(preview.value().front());
  for (std::size_t coordinate = 0; coordinate < actual.size(); ++coordinate)
    CHECK(actual[coordinate] == Approx(expected[coordinate]).margin(1e-7));
}

TEST_CASE("STEP reader rejects invalid empty unsupported-unit and degenerate geometry snapshots",
          "[geometry][step-import-reader]") {
  Fixture fixture;
  fixture.part(BRepPrimAPI_MakeBox(10, 20, 30).Shape(), "Box");
  const auto path = fixture.write();
  const auto original = fixture.bytes(path);
  SECTION("not STEP") {
    fixture.replace(path, "not a STEP model");
  }
  SECTION("empty STEP transfer") {
    fixture.replace(path,
                    "ISO-10303-21;\nHEADER;\nFILE_DESCRIPTION(('empty'),'2;1');\n"
                    "FILE_NAME('','','',(''),(''),'','');\nFILE_SCHEMA(('AUTOMOTIVE_DESIGN'));\n"
                    "ENDSEC;\nDATA;\nENDSEC;\nEND-ISO-10303-21;\n");
  }
  SECTION("unsupported declared length") {
    auto changed = original;
    const auto unit = changed.find(".MILLI.,.METRE.");
    REQUIRE(unit != std::string::npos);
    changed.replace(unit, std::string(".MILLI.,.METRE.").size(), "$,.SECOND.");
    fixture.replace(path, changed);
  }
  SECTION("ambiguous length context") {
    auto changed =
        std::regex_replace(original, std::regex(R"(GLOBAL_UNIT_ASSIGNED_CONTEXT\s*\(\((#[0-9]+),)"),
                           "GLOBAL_UNIT_ASSIGNED_CONTEXT(($1,$1,");
    REQUIRE(changed != original);
    fixture.replace(path, changed);
  }
  SECTION("degenerate solid geometry") {
    auto changed = std::regex_replace(original, std::regex(R"(CARTESIAN_POINT\('[^']*',\(10\.,)"),
                                      "CARTESIAN_POINT('',(0.,");
    REQUIRE(changed != original);
    fixture.replace(path, changed);
  }
  SECTION("non-finite model coordinates") {
    auto changed = original;
    const auto marker = changed.find("CARTESIAN_POINT('',(0.,0.,0.))");
    REQUIRE(marker != std::string::npos);
    changed.replace(marker, std::string("CARTESIAN_POINT('',(0.,0.,0.))").size(),
                    "CARTESIAN_POINT('',(1.E999,0.,0.))");
    fixture.replace(path, changed);
  }
  const auto before = fixture.bytes(path);
  const auto result = StepImportReader{}.read(fixture.source(path), fixture.root);
  REQUIRE_FALSE(result);
  CHECK(fixture.bytes(path) == before);
}

TEST_CASE("STEP reader rejects external references before reading an unregistered file",
          "[geometry][step-import-reader]") {
  Fixture fixture;
  fixture.part(BRepPrimAPI_MakeBox(10, 20, 30).Shape(), "External supplier product");
  const auto path = fixture.write("external.step", "MM", true);
  const auto before = fixture.bytes(path);
  const auto result = StepImportReader{}.read(fixture.source(path), fixture.root);
  REQUIRE_FALSE(result);
  CHECK(result.error().message().find("external STEP") != std::string::npos);
  CHECK(fixture.bytes(path) == before);
}

TEST_CASE("STEP reader rejects stale missing and symlink sources without publishing geometry",
          "[geometry][step-import-reader]") {
  Fixture fixture;
  fixture.part(BRepPrimAPI_MakeBox(10, 20, 30).Shape(), "Box");
  const auto path = fixture.write();
  const auto source = fixture.source(path);
  SECTION("digest changed") {
    fixture.replace(path, fixture.bytes(path) + "\n");
  }
  SECTION("source missing") {
    std::filesystem::remove(path);
  }
  SECTION("symlink source") {
    const auto other = fixture.root / "actual.step";
    std::filesystem::rename(path, other);
    std::filesystem::create_symlink(other.filename(), path);
  }
  CHECK_FALSE(StepImportReader{}.read(source, fixture.root));
}

TEST_CASE("STEP reader restores OCCT settings and processing policy after success and failure",
          "[geometry][step-import-reader]") {
  Fixture fixture;
  fixture.part(BRepPrimAPI_MakeBox(10, 20, 30).Shape(), "Box");
  const auto path = fixture.write();
  // Initialize exchange before recording settings; constructor initialization is not a leak.
  STEPControl_Reader initialized;
  const auto algorithm = XSAlgo::AlgoContainer();
  const double cascade_unit = UnitsMethods::GetCasCadeLengthUnit();
  const double step_unit = StepData_GlobalFactors::Intance().CascadeUnit();
  const std::array<double, 3> factors{StepData_GlobalFactors::Intance().LengthFactor(),
                                      StepData_GlobalFactors::Intance().PlaneAngleFactor(),
                                      StepData_GlobalFactors::Intance().SolidAngleFactor()};
  const bool naming = XCAFDoc_ShapeTool::AutoNaming();
  const std::string unit = Interface_Static::CVal("xstep.cascade.unit");
  const std::string context = Interface_Static::CVal("read.step.product.context");
  REQUIRE(Interface_Static::SetCVal("xstep.cascade.unit", "INCH"));
  REQUIRE(Interface_Static::SetCVal("read.step.product.context", "analysis"));
  for (bool bad : {false, true}) {
    if (bad)
      fixture.replace(path, "not STEP");
    auto result = StepImportReader{}.read(fixture.source(path), fixture.root);
    CHECK(static_cast<bool>(result) == !bad);
    CHECK(std::string(Interface_Static::CVal("xstep.cascade.unit")) == "INCH");
    CHECK(std::string(Interface_Static::CVal("read.step.product.context")) == "analysis");
    CHECK(XSAlgo::AlgoContainer() == algorithm);
    CHECK(UnitsMethods::GetCasCadeLengthUnit() == cascade_unit);
    CHECK(StepData_GlobalFactors::Intance().CascadeUnit() == step_unit);
    CHECK(StepData_GlobalFactors::Intance().LengthFactor() == factors[0]);
    CHECK(StepData_GlobalFactors::Intance().PlaneAngleFactor() == factors[1]);
    CHECK(StepData_GlobalFactors::Intance().SolidAngleFactor() == factors[2]);
    CHECK(XCAFDoc_ShapeTool::AutoNaming() == naming);
  }
  Interface_Static::SetCVal("xstep.cascade.unit", unit.c_str());
  Interface_Static::SetCVal("read.step.product.context", context.c_str());
}

TEST_CASE("STEP inspection preview rejects invalid graphs and non-rigid transforms",
          "[geometry][step-import-reader][step-import-transform-overflow]") {
  Fixture fixture;
  fixture.part(BRepPrimAPI_MakeBox(10, 20, 30).Shape(), "Box");
  const auto path = fixture.write();
  auto imported = StepImportReader{}.read(fixture.source(path), fixture.root);
  REQUIRE(imported);
  auto changed = imported.value();
  SECTION("missing definition") {
    changed.occurrences.front().product_definition = 99;
  }
  SECTION("definition cycle") {
    auto loop = changed.occurrences.front();
    loop.parent_definition = loop.product_definition;
    changed.occurrences.push_back(loop);
  }
  SECTION("no rooted occurrence") {
    changed.occurrences.clear();
  }
  SECTION("non-finite matrix") {
    changed.occurrences.front().transform.matrix[3] = std::numeric_limits<double>::infinity();
  }
  SECTION("scaled matrix") {
    changed.occurrences.front().transform.matrix[0] = 2;
  }
  SECTION("mirror matrix") {
    changed.occurrences.front().transform.matrix[0] = -1;
  }
  SECTION("finite direct transforms overflow only after hierarchy composition") {
    REQUIRE(changed.products.size() == 1);
    REQUIRE(changed.occurrences.size() == 1);
    changed.products.push_back(changed.products.front());
    changed.products.front().bodies.clear();
    changed.products.front().is_assembly = true;
    changed.products.front().source_kind.reset();
    changed.occurrences.front().transform.matrix[3] = 1e308;
    auto child = changed.occurrences.front();
    child.parent_definition = 0;
    child.product_definition = 1;
    changed.occurrences.push_back(child);
    CHECK(std::isfinite(changed.occurrences[0].transform.matrix[3]));
    CHECK(std::isfinite(changed.occurrences[1].transform.matrix[3]));
  }
  SECTION("finite occurrence and body-local placements overflow only when combined") {
    auto locally_placed = changed;
    locally_placed.occurrences.front().transform.matrix[3] = 1e308;
    auto local_shapes = StepImportReader{}.preview_shapes(locally_placed);
    REQUIRE(local_shapes);
    REQUIRE(local_shapes.value().size() == 1);
    const auto* local = OcctShapeView::get(local_shapes.value().front());
    REQUIRE(local != nullptr);
    CHECK(std::isfinite(local->Location().Transformation().Value(1, 4)));
    changed.products.front().bodies.front().shape = local_shapes.value().front();
    changed.occurrences.front().transform.matrix[3] = 1e308;
    CHECK(std::isfinite(changed.occurrences.front().transform.matrix[3]));
  }
  CHECK_FALSE(StepImportReader{}.preview_shapes(changed));
  CHECK(StepImportReader{}.preview_shapes(imported.value()));
}
