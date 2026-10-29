#pragma once

#include "blcad/core/part_document_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"

#include <BRep_Builder.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <STEPCAFControl_Controller.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <TDocStd_Document.hxx>
#include <TopoDS_Compound.hxx>
#include <UnitsMethods_LengthUnit.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace imported_body_test {
using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

template <typename T> T checked(Result<T> result) {
  if (!result)
    FAIL(result.error().object_id() + ": " + result.error().message());
  return std::move(result.value());
}

inline std::string bytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  REQUIRE(stream.good());
  return {std::istreambuf_iterator<char>(stream), {}};
}

struct Fixture {
  std::filesystem::path root;
  Fixture() {
    char name[] = "/tmp/blcad-imported-body-XXXXXX";
    const auto* created = ::mkdtemp(name);
    REQUIRE(created);
    root = created;
  }
  ~Fixture() {
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }
  std::filesystem::path path() const {
    return root / "supplier.step";
  }
  std::string source_bytes() const {
    return bytes(path());
  }
  void write_bytes(const std::string& value) const {
    std::ofstream stream(path(), std::ios::binary | std::ios::trunc);
    stream << value;
    REQUIRE(stream.good());
  }
  void write(const TopoDS_Shape& shape, const char* unit = "MM") const {
    Handle(TDocStd_Document) xde = new TDocStd_Document("BinXCAF");
    XCAFDoc_DocumentTool::Set(xde->Main());
    XCAFDoc_DocumentTool::SetLengthUnit(xde, 1, UnitsMethods_LengthUnit_Millimeter);
    REQUIRE(!XCAFDoc_DocumentTool::ShapeTool(xde->Main())->AddShape(shape, false, false).IsNull());
    STEPCAFControl_Controller::Init();
    const std::string previous = Interface_Static::CVal("write.step.unit");
    REQUIRE(Interface_Static::SetCVal("write.step.unit", unit));
    STEPCAFControl_Writer writer;
    const bool transferred = writer.Transfer(xde, STEPControl_AsIs);
    Interface_Static::SetCVal("write.step.unit", previous.c_str());
    REQUIRE(transferred);
    REQUIRE(writer.Write(path().c_str()) == IFSelect_RetDone);
  }
  PartDocument editable(bool reverse_outputs = false, const char* id = "editable") const {
    auto source = checked(StepImportSource::create(
        StepImportSourceId("supplier"), "Supplier", "supplier.step",
        checked(step_import_sha256(source_bytes())), StepImportMode::EditableBody));
    const auto imported = checked(StepImportReader{}.read(source, root));
    REQUIRE(imported.products.size() == 1);
    auto catalog = checked(
        StepImportTopology{}.create_catalog(ImportedTopologyCatalogId("catalog"), imported, 0));
    std::vector<ImportedBodyOutput> outputs;
    for (const auto& entry : catalog.entries()) {
      if (entry.descriptor.kind != ImportedTopologyKind::Body)
        continue;
      REQUIRE(entry.descriptor.body_kind);
      outputs.push_back({entry.id, BodyId("body." + std::to_string(outputs.size() + 1)),
                         *entry.descriptor.body_kind});
    }
    if (reverse_outputs)
      std::reverse(outputs.begin(), outputs.end());
    auto definition = checked(ImportedPartDefinition::create(source.id(), catalog.id(), outputs));
    auto feature = checked(ImportedBodyFeature::create(FeatureId("import.base"), "Imported base",
                                                       std::move(definition)));
    return checked(PartDocument::create_editable_import(DocumentId(id), "Editable supplier", source,
                                                        catalog, feature));
  }
};

inline TopoDS_Shape compound(std::vector<TopoDS_Shape> shapes) {
  TopoDS_Compound result;
  BRep_Builder builder;
  builder.MakeCompound(result);
  for (const auto& shape : shapes)
    builder.Add(result, shape);
  return result;
}

inline ShapeCache cache() {
  return checked(ShapeCache::create(ShapeCacheId("cache.imported")));
}
inline const TopoDS_Shape* topology(const GeometryShape& shape) {
  const auto* result = OcctShapeView::get(shape);
  REQUIRE(result);
  return result;
}
inline double volume(const GeometryShape& shape) {
  return RectangleExtrusionAdapter{}.summarize(shape).volume_mm3;
}
inline double body_volume(const ShapeCache& cache, const BodyId& body) {
  const auto* shape = cache.find_body_shape(body);
  REQUIRE(shape);
  return volume(*shape);
}
inline Parameter length(const std::string& id, double value) {
  return checked(
      Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(value))));
}
inline void accept_current(PartDocument& document, const Fixture& fixture) {
  const auto source = document.step_import_sources().front();
  REQUIRE(document.refresh_step_import_source(source,
                                              checked(step_import_sha256(fixture.source_bytes()))));
}
} // namespace imported_body_test
