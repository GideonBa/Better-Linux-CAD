#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/gui/gui_analysis_export_workbench.hpp"
#include "blcad/gui/gui_assembly_workbench.hpp"
#include "blcad/gui/gui_document_browser.hpp"
#include "blcad/gui/gui_step_import_workbench.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>

using namespace blcad;
using namespace blcad::gui;
namespace {
template <typename T> auto checked(T result) {
  INFO((result.has_error() ? result.error().message() : "ok"));
  REQUIRE(result);
  return result.value();
}
void write(const QString& path, const QByteArray& bytes) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::WriteOnly));
  REQUIRE(file.write(bytes) == bytes.size());
}
QByteArray bytes(const QString& path) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll();
}
std::string snapshot(const GuiDocumentSession& session) {
  return session.part_document()
             ? checked(serialize_part_document_to_json(*session.part_document()))
             : checked(serialize_project_to_json(*session.project()));
}
void create(GuiDocumentSession& session, bool project) {
  if (project)
    REQUIRE(session.create_project(DocumentId("project.source"), "Source project"));
  else
    REQUIRE(session.create_part(DocumentId("part.source"), "Source part"));
}
QString filename(bool project) {
  return project ? "model.blcad.project.json" : "model.blcad.json";
}
StepImportSource register_source(GuiDocumentSession& session, const QTemporaryDir& directory,
                                 const char* path = "source.step") {
  write(directory.filePath(path), "ISO-10303-21;\nsource A\nEND-ISO-10303-21;\n");
  const GuiStepImportWorkbench workbench;
  auto preview =
      checked(workbench.prepare_register(session, "Plate", path, StepImportMode::Reference));
  REQUIRE(workbench.apply(session, preview));
  return preview.source;
}
void write_geometry(const QString& path, double width = 20) {
  auto shape = checked(geometry::RectangleExtrusionAdapter{}.make_extruded_rectangle(
      checked(Quantity::length_mm(width)), checked(Quantity::length_mm(10)),
      checked(Quantity::length_mm(5))));
  REQUIRE(geometry::StepExporter{}.write_step(shape, path.toStdString()));
}
void write_product_geometry(const QString& path,
                            const std::vector<std::pair<double, double>>& sizes) {
  auto part = checked(PartDocument::create(DocumentId("supplier.products"), "Supplier products"));
  REQUIRE(part.add_datum_plane(checked(DatumPlane::xy())));
  for (std::size_t i = 0; i < sizes.size(); ++i) {
    const auto suffix = std::to_string(i);
    for (const auto& [name, length] : std::vector<std::pair<std::string, double>>{
             {"width.", sizes[i].first}, {"height.", 10}, {"depth.", sizes[i].second}})
      REQUIRE(part.add_parameter(checked(Parameter::create_length(
          ParameterId(name + suffix), name + suffix, checked(Quantity::length_mm(length))))));
    const BodyId body("body." + suffix);
    REQUIRE(part.add_body(checked(Body::create(body, "Product " + suffix, BodyKind::Solid))));
    auto sketch = checked(Sketch::create(SketchId("sketch." + suffix), "Profile " + suffix,
                                         DatumPlaneId("datum.xy")));
    REQUIRE(sketch.add_profile(checked(RectangleProfile::create(ProfileId("profile." + suffix),
                                                                ParameterId("width." + suffix),
                                                                ParameterId("height." + suffix)))));
    REQUIRE(part.add_sketch(sketch));
    auto feature = checked(Feature::create_additive_extrude(FeatureId("feature." + suffix),
                                                            "Extrusion " + suffix, sketch.id(),
                                                            ParameterId("depth." + suffix)));
    feature = checked(feature.with_body_result_context(checked(
        FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt, body))));
    REQUIRE(part.add_feature(feature));
  }
  auto cache = checked(geometry::ShapeCache::create(ShapeCacheId("supplier.products")));
  REQUIRE(geometry::GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto exported =
      checked(geometry::StepExporter{}.write_part_step(part, cache, path.toStdString()));
  REQUIRE(exported.exported_body_count == sizes.size());
}
std::size_t product_index(const geometry::StepImportGeometry& imported, double width,
                          double depth) {
  for (std::size_t index = 0; index < imported.products.size(); ++index) {
    const auto& product = imported.products[index];
    if (!product.is_assembly && product.bodies.size() == 1) {
      const auto& bounds = product.bodies.front().bounds_mm;
      if (std::abs(bounds[3] - bounds[0] - width) < 1e-6 &&
          std::abs(bounds[5] - bounds[2] - depth) < 1e-6)
        return index;
    }
  }
  FAIL("Expected supplier product dimensions were not transferred");
  return 0;
}
} // namespace

TEST_CASE(
    "STEP geometry inspection provides posed bodies units and diagnostics without model intent",
    "[gui][step-import-workbench][step-import-inspection]") {
  const bool project = GENERATE(false, true);
  const auto mode = GENERATE(StepImportMode::Reference, StepImportMode::EditableBody);
  GuiDocumentSession session;
  create(session, project);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const GuiStepImportWorkbench workbench;
  write_geometry(directory.filePath("source.step"));
  CHECK_FALSE(workbench.inspect_geometry(session, "Supplier", "source.step", mode));
  REQUIRE(session.save_as(directory.filePath(filename(project)).toStdString()));
  const auto before = snapshot(session);
  const auto asset = bytes(directory.filePath("source.step"));
  const auto undo = session.can_undo();
  const auto redo = session.can_redo();
  const auto preview =
      checked(workbench.inspect_geometry(session, "Supplier", "source.step", mode));
  CHECK_FALSE(preview.registered_source);
  CHECK(preview.source.mode() == mode);
  CHECK(preview.report.state() == StepImportFreshness::Available);
  REQUIRE(preview.geometry.units.size() == 1);
  CHECK(preview.geometry.units.front().length_to_mm == Catch::Approx(1));
  REQUIRE_FALSE(preview.geometry.products.empty());
  CHECK(preview.geometry.products.front().source_kind == geometry::StepImportedShapeKind::Solid);
  CHECK(preview.geometry.body_count() == 1);
  REQUIRE(preview.preview_shapes.size() == 1);
  REQUIRE_FALSE(preview.geometry.diagnostics.empty());
  CHECK(
      geometry::RectangleExtrusionAdapter{}.summarize(preview.preview_shapes.front()).volume_mm3 ==
      Catch::Approx(1000).margin(1e-5));
  REQUIRE_FALSE(preview.geometry.products.front().bodies.empty());
  const auto& body = preview.geometry.products.front().bodies.front();
  CHECK(body.kind == BodyKind::Solid);
  CHECK(body.volume_mm3 == Catch::Approx(1000).margin(1e-5));
  CHECK(body.bounds_mm[3] - body.bounds_mm[0] == Catch::Approx(20).margin(1e-5));
  CHECK(body.bounds_mm[4] - body.bounds_mm[1] == Catch::Approx(10).margin(1e-5));
  CHECK(body.bounds_mm[5] - body.bounds_mm[2] == Catch::Approx(5).margin(1e-5));
  CHECK(checked(workbench.validate_inspection(session, preview)) == 1);
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.dirty());
  CHECK(session.can_undo() == undo);
  CHECK(session.can_redo() == redo);
  CHECK(bytes(directory.filePath("source.step")) == asset);
  REQUIRE(session.save());
  CHECK(workbench.validate_inspection(session, preview));
  REQUIRE(session.save_as(directory.filePath("renamed.blcad.json").toStdString()));
  CHECK_FALSE(workbench.validate_inspection(session, preview));
  CHECK(snapshot(session) == before);
}

TEST_CASE(
    "STEP geometry inspection rejects changed registered bytes until explicit checksum refresh",
    "[gui][step-import-workbench][step-import-inspection][step-import-freshness]") {
  GuiDocumentSession session;
  create(session, false);
  QTemporaryDir directory;
  REQUIRE(session.save_as(directory.filePath("model.blcad.json").toStdString()));
  const auto asset = directory.filePath("source.step");
  write_geometry(asset);
  const GuiStepImportWorkbench workbench;
  const auto registration = checked(
      workbench.prepare_register(session, "Supplier", "source.step", StepImportMode::Reference));
  REQUIRE(workbench.apply(session, registration));
  REQUIRE(session.save());
  const auto preview = checked(workbench.inspect_geometry(session, registration.source.id()));
  CHECK(preview.registered_source);
  const auto before = snapshot(session);
  const auto undo_label = std::string(session.undo_label());
  write_geometry(asset, 40);
  const auto changed_bytes = bytes(asset);
  CHECK_FALSE(workbench.validate_inspection(session, preview));
  CHECK_FALSE(workbench.inspect_geometry(session, registration.source.id()));
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.dirty());
  CHECK(session.undo_label() == undo_label);
  const auto refresh = checked(workbench.prepare_refresh(session, registration.source.id()));
  CHECK(snapshot(session) == before);
  REQUIRE(workbench.apply(session, refresh));
  const auto updated = checked(workbench.inspect_geometry(session, registration.source.id()));
  CHECK(
      geometry::RectangleExtrusionAdapter{}.summarize(updated.preview_shapes.front()).volume_mm3 ==
      Catch::Approx(2000).margin(1e-5));
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  CHECK_FALSE(workbench.validate_inspection(session, updated));
  REQUIRE(session.can_redo());
  CHECK_FALSE(workbench.inspect_geometry(session, registration.source.id()));
  REQUIRE(session.can_redo());
  CHECK(bytes(asset) == changed_bytes);
  REQUIRE(QFile::remove(asset));
  CHECK_FALSE(workbench.inspect_geometry(session, registration.source.id()));
  CHECK(snapshot(session) == before);
}

TEST_CASE(
    "Malformed STEP geometry leaves opaque source registration and inspection history separate",
    "[gui][step-import-workbench][step-import-inspection]") {
  GuiDocumentSession session;
  create(session, false);
  QTemporaryDir directory;
  REQUIRE(session.save_as(directory.filePath("model.blcad.json").toStdString()));
  write(directory.filePath("source.step"), "ISO-10303-21;\nnot transferable\nEND-ISO-10303-21;\n");
  const GuiStepImportWorkbench workbench;
  const auto before = snapshot(session);
  CHECK_FALSE(
      workbench.inspect_geometry(session, "Supplier", "source.step", StepImportMode::Reference));
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.can_undo());
  const auto registry = checked(
      workbench.prepare_register(session, "Supplier", "source.step", StepImportMode::Reference));
  REQUIRE(workbench.apply(session, registry));
  const auto registered = snapshot(session);
  CHECK_FALSE(workbench.inspect_geometry(session, registry.source.id()));
  CHECK(snapshot(session) == registered);
  CHECK(session.undo_label() == "Register STEP source");
}

TEST_CASE("Imported topology capture is an explicit atomic catalog transaction shared by Part and "
          "Project",
          "[gui][step-import-workbench][step-import-topology-workbench]") {
  const bool project = GENERATE(false, true);
  GuiDocumentSession session;
  create(session, project);
  QTemporaryDir directory;
  REQUIRE(session.save_as(directory.filePath(filename(project)).toStdString()));
  write_geometry(directory.filePath("source.step"));
  const auto asset = bytes(directory.filePath("source.step"));
  const GuiStepImportWorkbench workbench;
  const auto registry = checked(
      workbench.prepare_register(session, "Supplier", "source.step", StepImportMode::Reference));
  REQUIRE(workbench.apply(session, registry));
  REQUIRE(session.save());
  const auto before = snapshot(session);
  const auto undo = std::string(session.undo_label());
  auto preview = checked(workbench.prepare_topology(session, registry.source.id(), 0));
  CHECK_FALSE(preview.accepted_catalog);
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.dirty());
  CHECK(session.undo_label() == undo);
  REQUIRE_FALSE(preview.catalog.entries().empty());
  for (const auto& entry : preview.catalog.entries()) {
    const auto* resolved = preview.recovery.find(entry.id);
    REQUIRE(resolved);
    CHECK(resolved->status == ImportedTopologyRecoveryStatus::Resolved);
    REQUIRE_FALSE(resolved->shape.empty());
    REQUIRE_FALSE(resolved->capabilities.empty());
    auto reference = checked(ImportedTopologyReference::create(preview.catalog.id(), entry.id,
                                                               resolved->capabilities.front()));
    CHECK(geometry::StepImportTopology{}.resolve(preview.recovery, reference));
    const auto spelling = checked(make_imported_topology_target_spelling(reference));
    CHECK(checked(parse_imported_topology_target_spelling(spelling)) == reference);
  }
  auto forged = preview;
  forged.captured_product_index = 9999;
  CHECK_FALSE(workbench.apply_topology(session, forged));
  CHECK(snapshot(session) == before);
  REQUIRE(workbench.apply_topology(session, preview));
  const auto accepted = snapshot(session);
  CHECK(accepted != before);
  CHECK(session.undo_label() == "Capture imported topology catalog");
  CHECK_FALSE(workbench.apply_topology(session, preview));
  CHECK_FALSE(workbench.prepare_topology(session, registry.source.id(), 0));
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == accepted);
  REQUIRE(session.save());
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(session.file_path()));
  CHECK(snapshot(reopened) == accepted);
  const auto restored = checked(workbench.inspect_topology(reopened, preview.catalog.id()));
  CHECK(restored.accepted_catalog);
  CHECK(restored.catalog == preview.catalog);
  CHECK(checked(workbench.apply_topology(reopened, restored)) == 0);
  CHECK_FALSE(reopened.dirty());
  CHECK_FALSE(reopened.can_undo());
  const auto remove = [&](auto& document) {
    return document.remove_imported_topology_catalog(preview.catalog.id());
  };
  REQUIRE((project ? reopened.commit_project_transaction("Remove catalog", remove)
                   : reopened.commit_part_transaction("Remove catalog", remove)));
  REQUIRE(reopened.undo());
  REQUIRE(reopened.can_redo());
  const auto unchanged = checked(workbench.inspect_topology(reopened, preview.catalog.id()));
  CHECK(checked(workbench.apply_topology(reopened, unchanged)) == 0);
  CHECK(reopened.can_redo());
  CHECK(snapshot(reopened) == accepted);
  CHECK(bytes(directory.filePath("source.step")) == asset);
}

TEST_CASE("Imported topology Apply rejects an edited public preview targeting an already captured "
          "product",
          "[gui][step-import-workbench][step-import-topology-workbench][step-import-topology-"
          "duplicate]") {
  GuiDocumentSession session;
  create(session, false);
  QTemporaryDir directory;
  REQUIRE(session.save_as(directory.filePath("products.blcad.json").toStdString()));
  const auto path = directory.filePath("products.step");
  write_product_geometry(path, {{20, 5}, {40, 5}});
  const auto asset = bytes(path);
  const GuiStepImportWorkbench workbench;
  const auto registry = checked(
      workbench.prepare_register(session, "Products", "products.step", StepImportMode::Reference));
  REQUIRE(workbench.apply(session, registry));
  const auto inspected = checked(workbench.inspect_geometry(session, registry.source.id()));
  const auto first = product_index(inspected.geometry, 20, 5);
  const auto second = product_index(inspected.geometry, 40, 5);
  REQUIRE(first != second);
  const auto captured = checked(workbench.prepare_topology(session, registry.source.id(), first));
  REQUIRE(workbench.apply_topology(session, captured));
  CHECK_FALSE(workbench.prepare_topology(session, registry.source.id(), first));
  auto edited = checked(workbench.prepare_topology(session, registry.source.id(), second));
  const auto before = snapshot(session);
  const auto undo = std::string(session.undo_label());
  edited.catalog = checked(geometry::StepImportTopology{}.create_catalog(
      edited.catalog.id(), edited.inspection.geometry, first));
  edited.recovery =
      checked(geometry::StepImportTopology{}.recover(edited.catalog, edited.inspection.geometry));
  edited.captured_product_index = first;
  REQUIRE(workbench.validate_topology(session, edited));
  CHECK_FALSE(workbench.apply_topology(session, edited));
  CHECK(snapshot(session) == before);
  CHECK(session.undo_label() == undo);
  CHECK(session.part_document()->imported_topology_catalogs().size() == 1);
  CHECK(bytes(path) == asset);
}

TEST_CASE("Ambiguous captured products do not block a different product excluded by exact product "
          "evidence",
          "[gui][step-import-workbench][step-import-topology-workbench][step-import-topology-"
          "duplicate]") {
  const bool shares_trimmed_face = GENERATE(false, true);
  CAPTURE(shares_trimmed_face);
  const double width = shares_trimmed_face ? 20 : 40;
  const double depth = shares_trimmed_face ? 10 : 5;
  GuiDocumentSession session;
  create(session, false);
  QTemporaryDir directory;
  REQUIRE(session.save_as(directory.filePath("products.blcad.json").toStdString()));
  const auto path = directory.filePath("products.step");
  write_product_geometry(path, {{20, 5}, {20, 5}, {width, depth}});
  const auto asset = bytes(path);
  const GuiStepImportWorkbench workbench;
  const auto registry = checked(
      workbench.prepare_register(session, "Products", "products.step", StepImportMode::Reference));
  REQUIRE(workbench.apply(session, registry));
  const auto inspected = checked(workbench.inspect_geometry(session, registry.source.id()));
  REQUIRE(inspected.geometry.products.size() == 3);
  const auto repeated = product_index(inspected.geometry, 20, 5);
  const auto distinct = product_index(inspected.geometry, width, depth);
  const auto captured =
      checked(workbench.prepare_topology(session, registry.source.id(), repeated));
  bool ambiguous = false;
  for (const auto& entry : captured.recovery.entries)
    ambiguous = ambiguous || entry.status == ImportedTopologyRecoveryStatus::Ambiguous;
  REQUIRE(ambiguous);
  REQUIRE(workbench.apply_topology(session, captured));
  const auto before = snapshot(session);
  const auto undo = std::string(session.undo_label());
  const auto fresh = workbench.prepare_topology(session, registry.source.id(), distinct);
  INFO((fresh ? "prepared" : fresh.error().message()));
  REQUIRE(fresh);
  CHECK(snapshot(session) == before);
  CHECK(session.undo_label() == undo);
  REQUIRE(workbench.apply_topology(session, fresh.value()));
  CHECK(session.part_document()->imported_topology_catalogs().size() == 2);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  REQUIRE(session.redo());
  CHECK(session.part_document()->imported_topology_catalogs().size() == 2);
  CHECK(bytes(path) == asset);
}

TEST_CASE(
    "Imported topology preview fails stale Apply and retains baseline IDs after source refresh",
    "[gui][step-import-workbench][step-import-topology-workbench]") {
  GuiDocumentSession session;
  create(session, false);
  QTemporaryDir directory;
  REQUIRE(session.save_as(directory.filePath("model.blcad.json").toStdString()));
  const auto asset = directory.filePath("source.step");
  write_geometry(asset);
  const GuiStepImportWorkbench workbench;
  const auto registry = checked(
      workbench.prepare_register(session, "Supplier", "source.step", StepImportMode::EditableBody));
  REQUIRE(workbench.apply(session, registry));
  auto preview = checked(workbench.prepare_topology(session, registry.source.id(), 0));
  const auto before = snapshot(session);
  REQUIRE(session.save_as(directory.filePath("moved.blcad.json").toStdString()));
  CHECK_FALSE(workbench.apply_topology(session, preview));
  CHECK(snapshot(session) == before);
  preview = checked(workbench.prepare_topology(session, registry.source.id(), 0));
  REQUIRE(workbench.apply_topology(session, preview));
  const auto catalog = preview.catalog;
  const auto accepted = snapshot(session);
  write_geometry(asset, 40);
  CHECK_FALSE(workbench.inspect_topology(session, catalog.id()));
  CHECK_FALSE(workbench.validate_topology(session, preview));
  CHECK(snapshot(session) == accepted);
  const auto refresh = checked(workbench.prepare_refresh(session, registry.source.id()));
  REQUIRE(workbench.apply(session, refresh));
  const auto recovered = checked(workbench.inspect_topology(session, catalog.id()));
  CHECK(recovered.catalog == catalog);
  bool lost = false;
  for (const auto& entry : recovered.recovery.entries)
    if (entry.status != ImportedTopologyRecoveryStatus::Resolved) {
      lost = true;
      CHECK(entry.shape.empty());
      CHECK_FALSE(entry.descriptor);
    }
  CHECK(lost);
  const auto refreshed = snapshot(session);
  CHECK(checked(workbench.apply_topology(session, recovered)) == 0);
  CHECK(snapshot(session) == refreshed);
  CHECK(session.part_document()->find_imported_topology_catalog(catalog.id()) != nullptr);
}

TEST_CASE("STEP source tasks require Save and register exact portable intent with atomic history",
          "[gui][step-import-workbench][step-import-source]") {
  const bool project = GENERATE(false, true);
  GuiDocumentSession session;
  create(session, project);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const GuiStepImportWorkbench workbench;
  CHECK_FALSE(
      workbench.prepare_register(session, "Source", "source.step", StepImportMode::Reference));
  REQUIRE(session.save_as(directory.filePath(filename(project)).toStdString()));
  const auto before = snapshot(session);
  write(directory.filePath("source.step"), "source A");
  auto preview = checked(
      workbench.prepare_register(session, "Plate", "./source.step", StepImportMode::EditableBody));
  CHECK(preview.source.project_relative_asset_path() == "source.step");
  CHECK(preview.source.mode() == StepImportMode::EditableBody);
  CHECK(preview.report.state() == StepImportFreshness::Available);
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.can_undo());
  REQUIRE(session.save());
  REQUIRE(workbench.apply(session, preview));
  const auto after = snapshot(session);
  CHECK(after != before);
  CHECK(session.undo_label() == "Register STEP source");
  CHECK_FALSE(
      workbench.prepare_register(session, "Duplicate", "source.step", StepImportMode::Reference));
  auto statuses = checked(workbench.inspect_sources(session));
  REQUIRE(statuses.size() == 1);
  CHECK(statuses.front().report.state() == StepImportFreshness::Available);
  CHECK_FALSE(statuses.front().embedded_part);
  auto browser = GuiDocumentBrowser::build(session);
  const auto& owner_id = statuses.front().owner_document.value();
  const auto& source_id = preview.source.id().value();
  const auto id = std::string("step-source/") + (project ? "project/" : "part/") +
                  std::to_string(owner_id.size()) + ":" + owner_id + "/" +
                  std::to_string(source_id.size()) + ":" + source_id;
  const auto* node = browser.find(id);
  REQUIRE(node);
  CHECK(node->kind == GuiBrowserNodeKind::ExternalSource);
  CHECK(node->viewport_semantic_id.empty());
  for (const auto& property : node->properties)
    CHECK_FALSE(property.editable);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == after);
  REQUIRE(session.save());
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(session.file_path()));
  CHECK(snapshot(reopened) == after);
  auto unchanged = checked(workbench.prepare_refresh(reopened, preview.source.id()));
  CHECK(checked(workbench.apply(reopened, unchanged)) == 0);
  CHECK_FALSE(reopened.can_undo());
  const auto removal = checked(workbench.prepare_remove(reopened, preview.source.id()));
  REQUIRE(workbench.apply(reopened, removal));
  REQUIRE(checked(workbench.inspect_sources(reopened)).empty());
  REQUIRE(reopened.undo());
  REQUIRE(reopened.can_redo());
  const auto before_refresh = snapshot(reopened);
  unchanged = checked(workbench.prepare_refresh(reopened, preview.source.id()));
  CHECK(checked(workbench.apply(reopened, unchanged)) == 0);
  CHECK(snapshot(reopened) == before_refresh);
  REQUIRE(reopened.can_redo());
  REQUIRE(reopened.redo());
  CHECK(checked(workbench.inspect_sources(reopened)).empty());
  CHECK(bytes(directory.filePath("source.step")) == "source A");
}

TEST_CASE(
    "STEP source refresh rereads bytes rejects stale intent and accepts only inspected digest",
    "[gui][step-import-workbench][step-import-freshness]") {
  const bool project = GENERATE(false, true);
  GuiDocumentSession session;
  create(session, project);
  QTemporaryDir directory;
  REQUIRE(session.save_as(directory.filePath(filename(project)).toStdString()));
  const auto source = register_source(session, directory);
  const GuiStepImportWorkbench workbench;
  const auto before = snapshot(session);
  write(directory.filePath("source.step"), "source B");
  auto preview = checked(workbench.prepare_refresh(session, source.id()));
  CHECK(preview.report.state() == StepImportFreshness::Changed);
  CHECK(snapshot(session) == before);
  write(directory.filePath("source.step"), "source C");
  CHECK_FALSE(workbench.apply(session, preview));
  CHECK(snapshot(session) == before);
  preview = checked(workbench.prepare_refresh(session, source.id()));
  REQUIRE(workbench.apply(session, preview));
  CHECK(checked(workbench.inspect(session, source.id())).state() == StepImportFreshness::Available);
  const auto refreshed = snapshot(session);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  CHECK(checked(workbench.inspect(session, source.id())).state() == StepImportFreshness::Changed);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == refreshed);
  auto removal = checked(workbench.prepare_remove(session, source.id()));
  register_source(session, directory, "second.step");
  CHECK_FALSE(workbench.apply(session, removal));
  auto forged = checked(workbench.prepare_remove(session, source.id()));
  forged.source =
      checked(StepImportSource::create(source.id(), "Forged", "source.step",
                                       forged.source.sha256_digest(), StepImportMode::Reference));
  CHECK_FALSE(workbench.apply(session, forged));
  CHECK(bytes(directory.filePath("source.step")) == "source C");
}

TEST_CASE("Missing and unreadable STEP metadata stays openable for explicit recovery",
          "[gui][step-import-workbench][step-import-freshness]") {
  const bool project = GENERATE(false, true);
  GuiDocumentSession session;
  create(session, project);
  QTemporaryDir directory;
  REQUIRE(session.save_as(directory.filePath(filename(project)).toStdString()));
  const auto source = register_source(session, directory);
  REQUIRE(session.save());
  REQUIRE(QFile::remove(directory.filePath("source.step")));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(session.file_path()));
  const auto before = snapshot(reopened);
  const GuiStepImportWorkbench workbench;
  CHECK(reopened.derived_results_fresh());
  CHECK(checked(workbench.inspect(reopened, source.id())).state() == StepImportFreshness::Missing);
  REQUIRE_FALSE(reopened.diagnostics().empty());
  CHECK(reopened.diagnostics().back().severity == GuiDiagnosticSeverity::Warning);
  CHECK_FALSE(workbench.prepare_refresh(reopened, source.id()));
  REQUIRE(reopened.save());
  auto missing = checked(workbench.prepare_remove(reopened, source.id()));
  REQUIRE(QDir().mkdir(directory.filePath("source.step")));
  CHECK_FALSE(workbench.apply(reopened, missing));
  CHECK(snapshot(reopened) == before);
  CHECK(checked(workbench.inspect(reopened, source.id())).state() ==
        StepImportFreshness::Unreadable);
  auto unreadable = checked(workbench.prepare_remove(reopened, source.id()));
  REQUIRE(workbench.apply(reopened, unreadable));
  CHECK(checked(workbench.inspect_sources(reopened)).empty());
  REQUIRE(reopened.undo());
  CHECK(snapshot(reopened) == before);
  REQUIRE(QDir().rmdir(directory.filePath("source.step")));
  write(directory.filePath("source.step"), "Recovered source");
  auto refresh = checked(workbench.prepare_refresh(reopened, source.id()));
  REQUIRE(workbench.apply(reopened, refresh));
  CHECK(checked(workbench.inspect(reopened, source.id())).state() ==
        StepImportFreshness::Available);
}

TEST_CASE("Save As requires matching relocated assets and invalidates old source previews",
          "[gui][step-import-workbench][step-import-json]") {
  const bool project = GENERATE(false, true);
  GuiDocumentSession session;
  create(session, project);
  QTemporaryDir original, moved;
  REQUIRE(session.save_as(original.filePath(filename(project)).toStdString()));
  const auto source = register_source(session, original);
  REQUIRE(session.save());
  const auto old_path = session.file_path();
  const auto before = snapshot(session);
  const GuiStepImportWorkbench workbench;
  const auto preview = checked(workbench.prepare_remove(session, source.id()));
  const auto target = moved.filePath(filename(project));
  CHECK_FALSE(session.save_as(target.toStdString()));
  CHECK_FALSE(QFile::exists(target));
  CHECK(session.file_path() == old_path);
  write(moved.filePath("source.step"), "Wrong source");
  CHECK_FALSE(session.save_as(target.toStdString()));
  CHECK(snapshot(session) == before);
  write(moved.filePath("source.step"), bytes(original.filePath("source.step")));
  REQUIRE(session.save_as(target.toStdString()));
  CHECK(snapshot(session) == before);
  CHECK_FALSE(workbench.apply(session, preview));
  CHECK_FALSE(session.dirty());
  REQUIRE(QFile::remove(original.filePath("source.step")));
  CHECK(checked(workbench.inspect(session, source.id())).state() == StepImportFreshness::Available);
}

TEST_CASE("Document and STEP export writers preserve source assets and aliases",
          "[gui][step-import-workbench][step-import-source]") {
  GuiDocumentSession session;
  create(session, false);
  QTemporaryDir directory;
  const auto document = directory.filePath("model.blcad.json");
  REQUIRE(session.save_as(document.toStdString()));
  const GuiStepImportWorkbench workbench;
  CHECK_FALSE(
      workbench.prepare_register(session, "Self", "model.blcad.json", StepImportMode::Reference));
  register_source(session, directory);
  const auto content = bytes(directory.filePath("source.step"));
  std::filesystem::create_hard_link(directory.filePath("source.step").toStdString(),
                                    directory.filePath("alias.step").toStdString());
  for (const auto* name : {"source.step", "alias.step"}) {
    const auto path = directory.filePath(name).toStdString();
    CHECK_FALSE(session.save_as(path));
    CHECK_FALSE(GuiAnalysisExportWorkbench{}.preflight_step_export(
        session, {GuiStepExportMode::PartMultiBody, path}));
    CHECK(bytes(directory.filePath(name)) == content);
  }
  REQUIRE(QFile::remove(directory.filePath("source.step")));
  CHECK_FALSE(session.save_as(directory.filePath("source.step").toStdString()));
  CHECK_FALSE(QFile::exists(directory.filePath("source.step")));
  write(directory.filePath("other.blcad.json.sketch-constraints.json"), "Sidecar source");
  auto sidecar = checked(workbench.prepare_register(
      session, "Sidecar", "other.blcad.json.sketch-constraints.json", StepImportMode::Reference));
  REQUIRE(workbench.apply(session, sidecar));
  CHECK_FALSE(session.save_as(directory.filePath("other.blcad.json").toStdString()));
  CHECK(bytes(directory.filePath("other.blcad.json.sketch-constraints.json")) == "Sidecar source");
}

TEST_CASE("Assembly definition import verifies embedded source roots and protects their assets",
          "[gui][step-import-workbench][step-import-source]") {
  GuiDocumentSession part;
  create(part, false);
  QTemporaryDir original, target;
  REQUIRE(part.save_as(original.filePath("part.blcad.json").toStdString()));
  register_source(part, original);
  GuiDocumentSession project;
  create(project, true);
  const GuiAssemblyWorkbench assembly;
  CHECK_FALSE(assembly.add_part(project, *part.part_document()));
  REQUIRE(project.save_as(target.filePath("model.blcad.project.json").toStdString()));
  CHECK_FALSE(assembly.add_part(project, *part.part_document()));
  write(target.filePath("source.step"), bytes(original.filePath("source.step")));
  REQUIRE(assembly.add_part(project, *part.part_document()));
  const auto inspected = checked(GuiStepImportWorkbench{}.inspect_sources(project));
  REQUIRE(inspected.size() == 1);
  CHECK(inspected.front().owner_document == part.part_document()->id());
  CHECK(inspected.front().embedded_part);
  CHECK_FALSE(
      validate_step_import_output_path(project, target.filePath("source.step").toStdString()));
  const auto browser = GuiDocumentBrowser::build(project);
  const auto& id = inspected.front().report.source().id().value();
  REQUIRE(browser.find("step-source/part/11:part.source/" + std::to_string(id.size()) + ":" + id));
}

TEST_CASE("STEP source presentation distinguishes registry kinds and delimited persistent ids",
          "[gui][step-import-workbench][step-import-source]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  auto project =
      checked(Project::create(DocumentId("a"), "Root project",
                              checked(AssemblyDocument::create(DocumentId("root"), "Root"))));
  const auto add_source = [&](auto& owner, const char* id, const char* path, const char* name) {
    write(directory.filePath(path), name);
    REQUIRE(owner.add_step_import_source(checked(
        StepImportSource::create(StepImportSourceId(id), name, path,
                                 checked(step_import_sha256(name)), StepImportMode::Reference))));
  };
  add_source(project, "b/c", "root.step", "Project source");
  auto same_id_part = checked(PartDocument::create(DocumentId("a"), "Same-id Part"));
  add_source(same_id_part, "b/c", "part.step", "Part source");
  REQUIRE(project.add_part_document(std::move(same_id_part)));
  auto slashed_part = checked(PartDocument::create(DocumentId("a/b"), "Slashed Part"));
  add_source(slashed_part, "c", "slashed.step", "Slashed source");
  REQUIRE(project.add_part_document(std::move(slashed_part)));
  const auto serialized = checked(serialize_project_to_json(project));
  const auto path = directory.filePath("model.blcad.project.json");
  write(path, QByteArray::fromStdString(serialized));
  GuiDocumentSession session;
  REQUIRE(session.open_file(path.toStdString()));
  const auto before = snapshot(session);
  const auto statuses = checked(GuiStepImportWorkbench{}.inspect_sources(session));
  REQUIRE(statuses.size() == 3);
  CHECK(statuses[0].owner_document == DocumentId("a"));
  CHECK_FALSE(statuses[0].embedded_part);
  CHECK(statuses[1].owner_document == DocumentId("a"));
  CHECK(statuses[1].embedded_part);
  CHECK(statuses[2].owner_document == DocumentId("a/b"));
  CHECK(statuses[2].embedded_part);
  for (const auto& status : statuses)
    CHECK(status.report.state() == StepImportFreshness::Available);
  const auto browser = GuiDocumentBrowser::build(session);
  const auto check_node = [&](const char* semantic_id, const char* owner, const char* source_id,
                              const char* name, const char* asset_path) {
    const auto* node = browser.find(semantic_id);
    REQUIRE(node);
    CHECK(node->kind == GuiBrowserNodeKind::ExternalSource);
    CHECK(node->owner_document_id == owner);
    CHECK(node->label == name);
    CHECK_FALSE(node->selection_kind);
    CHECK(node->viewport_semantic_id.empty());
    std::size_t matched = 0;
    for (const auto& property : node->properties) {
      CHECK_FALSE(property.editable);
      if (property.key == "id") {
        CHECK(property.value == source_id);
        ++matched;
      }
      if (property.key == "path") {
        CHECK(property.value == asset_path);
        ++matched;
      }
    }
    CHECK(matched == 2);
  };
  check_node("step-source/project/1:a/3:b/c", "a", "b/c", "Project source", "root.step");
  check_node("step-source/part/1:a/3:b/c", "a", "b/c", "Part source", "part.step");
  check_node("step-source/part/3:a/b/1:c", "a/b", "c", "Slashed source", "slashed.step");
  CHECK(snapshot(session) == before);
  CHECK(before == serialized);
  CHECK_FALSE(session.dirty());
  CHECK_FALSE(session.can_undo());
}
