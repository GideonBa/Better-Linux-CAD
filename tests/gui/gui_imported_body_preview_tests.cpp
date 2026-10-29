#include "blcad/core/part_document_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/step_editable_commands.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/geometry/step_import_topology.hpp"
#include "blcad/gui/gui_interactive_extrude_revolve.hpp"
#include "blcad/gui/gui_part_operations_workbench.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace blcad;
using namespace blcad::gui;
namespace {
template <typename T> auto checked(T result) {
  INFO((result.has_error() ? result.error().message() : "ok"));
  REQUIRE(result);
  return result.value();
}
QByteArray bytes(const QString& path) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll();
}
} // namespace

TEST_CASE("Imported Body interactive previews share the saved source root and reject stale bytes",
          "[gui][imported-body-feature][imported-body-preview]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto root = std::filesystem::path(directory.path().toStdString());
  const auto asset_path = directory.filePath("supplier.step");
  auto stock = checked(geometry::RectangleExtrusionAdapter{}.make_extruded_rectangle(
      checked(Quantity::length_mm(20)), checked(Quantity::length_mm(10)),
      checked(Quantity::length_mm(5))));
  REQUIRE(geometry::StepExporter{}.write_step(stock, asset_path.toStdString()));
  const auto asset = checked(inspect_step_import_asset(root, "supplier.step"));
  REQUIRE(asset.observed_sha256_digest());
  const auto source = checked(StepImportSource::create(
      StepImportSourceId("source.stock"), "Supplier stock", "supplier.step",
      *asset.observed_sha256_digest(), StepImportMode::EditableBody));
  const auto imported = checked(geometry::StepImportReader{}.read(source, root));
  const auto product =
      std::find_if(imported.products.begin(), imported.products.end(),
                   [](const auto& value) { return !value.is_assembly && !value.bodies.empty(); });
  REQUIRE(product != imported.products.end());
  const auto catalog = checked(geometry::StepImportTopology{}.create_catalog(
      ImportedTopologyCatalogId("catalog.stock"), imported,
      static_cast<std::size_t>(product - imported.products.begin())));
  auto part = checked(geometry::StepEditableCommands{}.create_definition(
      source, catalog, DocumentId("part.editable.preview"), "Editable stock",
      FeatureId("feature.imported.base"), root));
  const auto document = directory.filePath("editable.blcad.json").toStdString();
  REQUIRE(write_part_document_json_file(part, document));
  GuiDocumentSession session;
  REQUIRE(session.open_file(document));
  const auto body = session.part_document()->bodies().front().id();
  REQUIRE(session.commit_part_transaction("Create downstream profile", [](PartDocument& candidate) {
    for (const auto& [id, value] :
         std::vector<std::pair<std::string, double>>{{"width", 3}, {"height", 4}, {"depth", 8}}) {
      auto added = candidate.add_parameter(checked(Parameter::create_length(
          ParameterId("join." + id), id, checked(Quantity::length_mm(value)))));
      if (!added)
        return added;
    }
    auto sketch =
        checked(Sketch::create(SketchId("sketch.join"), "Join profile", DatumPlaneId("datum.xy")));
    REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
        ProfileId("profile.join"), ParameterId("join.width"), ParameterId("join.height")))));
    return candidate.add_sketch(std::move(sketch));
  }));
  const auto before = checked(serialize_part_document_to_json(*session.part_document()));
  const std::string before_history(session.undo_label());
  GuiInteractiveExtrudeController controller;
  REQUIRE(controller.begin(session, SketchId("sketch.join"), ProfileId("profile.join"),
                           FeatureId("feature.join"), "Imported stock boss",
                           ParameterId("join.depth"), body));
  controller.set_operation(FeatureBodyOperationMode::Join, body);
  CHECK_FALSE(checked(controller.preview_shape(session)).empty());
  CHECK(checked(serialize_part_document_to_json(*session.part_document())) == before);
  CHECK(session.undo_label() == before_history);
  REQUIRE(session.part_shape_cache()->find_body_result(body));
  CHECK(session.part_shape_cache()->find_body_result(body)->source_feature_id ==
        FeatureId("feature.imported.base"));
  REQUIRE(controller.apply(session));
  REQUIRE(session.part_shape_cache()->find_body_result(body));
  CHECK(session.part_shape_cache()->find_body_result(body)->source_feature_id ==
        FeatureId("feature.join"));
  const auto committed = checked(serialize_part_document_to_json(*session.part_document()));
  const std::string history(session.undo_label());
  auto candidate = *session.part_document();
  REQUIRE(
      candidate.set_parameter_value(ParameterId("join.depth"), checked(Quantity::length_mm(9))));
  auto preview =
      checked(blcad::gui::detail::recompute_preview_geometry(session, candidate, {"join.depth"}));
  CHECK(preview.feature_shape_count() == 2);
  REQUIRE(preview.find_body_result(body));
  CHECK(preview.find_body_result(body)->source_feature_id == FeatureId("feature.join"));
  CHECK(checked(serialize_part_document_to_json(*session.part_document())) == committed);
  const auto output = directory.filePath("edited.step");
  REQUIRE(GuiPartOperationsWorkbench{}.export_visible_bodies_step(session, output.toStdString()));
  const auto accepted_bytes = bytes(asset_path);
  CHECK_FALSE(
      GuiPartOperationsWorkbench{}.export_visible_bodies_step(session, asset_path.toStdString()));
  CHECK(bytes(asset_path) == accepted_bytes);
  QFile changed(asset_path);
  REQUIRE(changed.open(QIODevice::Append));
  REQUIRE(changed.write("\n/* external supplier revision */\n") > 0);
  changed.close();
  const auto changed_bytes = bytes(asset_path);
  REQUIRE(session.derived_results_fresh());
  CHECK_FALSE(blcad::gui::detail::recompute_preview_geometry(session, candidate, {"join.depth"}));
  CHECK_FALSE(GuiSketchWorkbench{}.plane_view(session, SketchId("sketch.join")));
  const auto stale_output = directory.filePath("stale.step");
  CHECK_FALSE(
      GuiPartOperationsWorkbench{}.export_visible_bodies_step(session, stale_output.toStdString()));
  CHECK_FALSE(QFile::exists(stale_output));
  CHECK(checked(serialize_part_document_to_json(*session.part_document())) == committed);
  CHECK(session.undo_label() == history);
  CHECK(bytes(asset_path) == changed_bytes);
}
