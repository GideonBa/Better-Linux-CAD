#include "../support/material_physical_fixture.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/gui/gui_materials.hpp"
#include "blcad/gui/gui_physical_properties.hpp"

#include <QTemporaryDir>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <iterator>
#include <locale>

using namespace blcad;
using namespace blcad::gui;
namespace fixture = blcad::test::material_physical_fixture;
namespace resources = blcad::test::engineering_fixture;
namespace {
template <typename T> T checked(Result<T> result) {
  INFO((result ? std::string{} : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
GuiDocumentSession session(PartDocument p) {
  GuiDocumentSession s;
  REQUIRE(s.create_part(p.id(), p.name()));
  REQUIRE(s.commit_part_transaction("Seed Part", [&](auto& candidate) {
    candidate = p;
    return Result<std::size_t>::success(1);
  }));
  return s;
}
GuiDocumentSession session(Project p) {
  GuiDocumentSession s;
  REQUIRE(s.create_project(p.id(), p.name()));
  REQUIRE(s.commit_project_transaction("Seed Project", [&](auto& candidate) {
    candidate = p;
    return Result<std::size_t>::success(1);
  }));
  return s;
}
std::string source(const GuiDocumentSession& s) {
  return s.part_document() ? checked(serialize_part_document_to_json(*s.part_document()))
                           : checked(serialize_project_to_json(*s.project()));
}
std::string file(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
Catch::Approx approximately(double value) {
  return Catch::Approx(value).epsilon(1e-9).margin(1e-12);
}
PartDocument imported(const std::filesystem::path& root, bool reference) {
  auto native = fixture::part();
  auto cache = checked(geometry::ShapeCache::create(ShapeCacheId("controller.supplier")));
  REQUIRE(geometry::GeometryRecomputeExecutor{}.execute_document(native, cache));
  const auto path = (root / "supplier.step").string();
  REQUIRE(geometry::StepExporter{}.write_part_step(native, cache, path));
  const auto bytes = file(path);
  const auto supplier = checked(StepImportSource::create(
      StepImportSourceId("supplier"), "Supplier", "supplier.step",
      checked(step_import_sha256(bytes)),
      reference ? StepImportMode::Reference : StepImportMode::EditableBody));
  const auto imported_geometry = checked(geometry::StepImportReader{}.read(supplier, root));
  const auto catalog = checked(geometry::StepImportTopology{}.create_catalog(
      ImportedTopologyCatalogId("catalog"), imported_geometry, 0));
  std::vector<ImportedBodyOutput> outputs;
  for (const auto& entry : catalog.entries())
    if (entry.descriptor.kind == ImportedTopologyKind::Body)
      outputs.push_back({entry.id, BodyId("import.body." + std::to_string(outputs.size())),
                         *entry.descriptor.body_kind});
  const auto definition =
      checked(ImportedPartDefinition::create(supplier.id(), catalog.id(), outputs));
  if (reference)
    return checked(PartDocument::create_reference(DocumentId("reference"), "Reference", supplier,
                                                  catalog, definition));
  const auto feature =
      checked(ImportedBodyFeature::create(FeatureId("imported"), "Imported", definition));
  auto result = checked(PartDocument::create_editable_import(DocumentId("editable"), "Editable",
                                                             supplier, catalog, feature));
  resources::annotate(result);
  return result;
}
} // namespace

TEST_CASE(
    "Material controller stages typed changes without leaking through Save and accepts one Undo",
    "[gui][parity-b151][material-controller]") {
  for (bool authored : {false, true}) {
    DYNAMIC_SECTION("authored " << authored) {
      QTemporaryDir directory;
      auto s = session(fixture::part("material.part", true, authored));
      const auto path = directory.filePath("material.blcad.json").toStdString();
      REQUIRE(s.save_as(path));
      const auto before = source(s), saved = file(path);
      const auto generation = s.part_document()->executable_generation();
      GuiMaterialController editor;
      REQUIRE(editor.begin(s));
      REQUIRE(editor.stage_assignment(s, s.part_document()->id(), {},
                                      resources::key("synthetic.material", "r2")));
      CHECK(editor.changed());
      CHECK(source(s) == before);
      CHECK_FALSE(s.dirty());
      REQUIRE(s.save());
      CHECK(file(path) == saved);
      REQUIRE(editor.current(s));
      const auto applied = checked(editor.apply(s, "Assign material"));
      CHECK(applied == 1);
      CHECK_FALSE(editor.active());
      CHECK(s.dirty());
      CHECK(s.undo_label() == "Assign material");
      CHECK(s.part_document()->executable_generation() == generation);
      CHECK(s.part_document()->default_material() == resources::key("synthetic.material", "r2"));
      CHECK(s.part_document()->effective_material_for_body(BodyId("body.second"))->key() ==
            resources::key("synthetic.material", "r2"));
      const auto after = source(s);
      REQUIRE(s.undo());
      CHECK(source(s) == before);
      CHECK_FALSE(s.dirty());
      REQUIRE(s.redo());
      CHECK(source(s) == after);
      REQUIRE(s.save());
      GuiDocumentSession opened;
      REQUIRE(opened.open_file(path));
      CHECK(source(opened) == after);
    }
  }
}

TEST_CASE("Identical material intent and Cancel preserve history revision and dirty state",
          "[gui][parity-b151][material-controller]") {
  auto s = session(fixture::part());
  QTemporaryDir directory;
  REQUIRE(s.save_as(directory.filePath("part.blcad.json").toStdString()));
  const auto before = source(s), label = std::string(s.undo_label());
  const auto revision = s.model_revision();
  GuiMaterialController editor;
  REQUIRE(editor.begin(s));
  REQUIRE(editor.stage_material(s, resources::material()));
  REQUIRE(editor.stage_assignment(s, s.part_document()->id(), {}, resources::key()));
  CHECK_FALSE(editor.changed());
  CHECK(checked(editor.apply(s, "Identical material")) == 0);
  CHECK(s.model_revision() == revision);
  CHECK(s.undo_label() == label);
  CHECK_FALSE(s.dirty());
  REQUIRE(editor.begin(s));
  REQUIRE(editor.stage_assignment(s, s.part_document()->id(), BodyId("body.base"),
                                  resources::key("synthetic.material", "r2")));
  CHECK(editor.changed());
  editor.cancel();
  CHECK(source(s) == before);
  CHECK(s.model_revision() == revision);
  CHECK_FALSE(s.dirty());
}

TEST_CASE(
    "Material revision candidates update explicit Project members and roll back late failures",
    "[gui][parity-b151][material-controller]") {
  auto s = session(fixture::project(true));
  const auto before = source(s);
  GuiMaterialController editor;
  REQUIRE(editor.begin(s));
  REQUIRE(editor.stage_revision(s, resources::key(), resources::material("r3", 7000),
                                {DocumentId("material.part")}));
  const auto candidate = checked(serialize_project_to_json(*editor.project()));
  CHECK(editor.project()->find_part_document(DocumentId("material.part"))->default_material() ==
        resources::key("synthetic.material", "r3"));
  CHECK(editor.project()->find_part_document(DocumentId("unselected"))->default_material() ==
        resources::key());
  CHECK_FALSE(editor.stage_revision(s, resources::key("synthetic.material", "r3"),
                                    resources::material("r4", 8000),
                                    {DocumentId("material.part"), DocumentId("missing")}));
  CHECK(checked(serialize_project_to_json(*editor.project())) == candidate);
  CHECK(source(s) == before);
  REQUIRE(editor.apply(s, "Adopt selected material"));
  CHECK(source(s) == candidate);
  REQUIRE(s.undo());
  CHECK(source(s) == before);
  REQUIRE(s.redo());
  CHECK(source(s) == candidate);
}

TEST_CASE("Material library assignment copies pinned root snapshots and removal remains atomic",
          "[gui][parity-b151][material-controller]") {
  auto s = session(fixture::project(true));
  GuiMaterialController editor;
  REQUIRE(editor.begin(s));
  REQUIRE(editor.stage_material(s, resources::material("r3", 7000)));
  REQUIRE(editor.stage_assignment(s, DocumentId("material.part"), BodyId("body.base"),
                                  resources::key("synthetic.material", "r3")));
  const auto candidate = checked(serialize_project_to_json(*editor.project()));
  CHECK(editor.project()
            ->find_part_document(DocumentId("material.part"))
            ->engineering_resources()
            .find_material(resources::key("synthetic.material", "r3")));
  CHECK_FALSE(editor.stage_remove(s, resources::key("synthetic.material", "r3"),
                                  {DocumentId("material.part")}));
  CHECK(checked(serialize_project_to_json(*editor.project())) == candidate);
  CHECK_FALSE(editor.stage_assignment(s, DocumentId("foreign"), {}, resources::key()));
  CHECK_FALSE(
      editor.stage_assignment(s, DocumentId("material.part"), BodyId("missing"), resources::key()));
  REQUIRE(editor.stage_assignment(s, DocumentId("material.part"), BodyId("body.base"), {}));
  REQUIRE(editor.stage_remove(s, resources::key("synthetic.material", "r3"),
                              {DocumentId("material.part")}));
  CHECK_FALSE(editor.changed());
  CHECK(checked(editor.apply(s, "No remaining changes")) == 0);
}

TEST_CASE("Material controllers reject stale lifetimes and foreign tasks while prefix edits retain "
          "outer Save",
          "[gui][parity-b151][material-controller]") {
  auto s = session(fixture::part("material.part", false, true));
  GuiMaterialController editor;
  REQUIRE(s.task().begin("foreign"));
  CHECK_FALSE(editor.begin(s));
  REQUIRE(s.task().cancel());
  REQUIRE(editor.begin(s));
  const auto feature = *s.part_document()->find_feature(FeatureId("base"));
  REQUIRE(s.part_document()->remove_feature(feature.id()));
  REQUIRE(s.part_document()->add_feature(feature));
  CHECK_FALSE(editor.current(s));
  CHECK_FALSE(editor.stage_assignment(s, s.part_document()->id(), {}, resources::key()));
  editor.cancel();
  REQUIRE(s.recompute());
  QTemporaryDir directory;
  const auto path = directory.filePath("part.blcad.json").toStdString();
  REQUIRE(s.save_as(path));
  const auto before = source(s), saved = file(path);
  REQUIRE(s.begin_part_history_context(1));
  REQUIRE(editor.begin(s));
  REQUIRE(editor.stage_assignment(s, s.part_document()->id(), {},
                                  resources::key("synthetic.material", "r2")));
  REQUIRE(editor.apply(s, "Draft material"));
  CHECK_FALSE(s.derived_results_fresh());
  CHECK(s.history_prefix_ready());
  REQUIRE(s.save());
  CHECK(file(path) == saved);
  s.cancel_part_workflow();
  CHECK(source(s) == before);
}

TEST_CASE(
    "Material metadata preserves expression-driven geometry and reuses integrated physical results",
    "[gui][parity-b151][material-controller][physical-properties-controller]") {
  auto p = fixture::part();
  REQUIRE(p.add_expression_parameter(ParameterId("computed"), "Computed", ParameterType::Length,
                                     "depth * 2"));
  const auto context = blcad::test::history_fixture::new_body(p, "computed");
  auto feature =
      checked(Feature::create_additive_extrude(FeatureId("computed.feature"), "Computed extrusion",
                                               SketchId("profile"), ParameterId("computed")));
  REQUIRE(p.add_feature(checked(feature.with_body_result_context(context))));
  auto s = session(std::move(p));
  const auto shape = *s.part_shape_cache()->find_body_shape(BodyId("body.computed"));
  GuiPhysicalPropertiesController inspect;
  REQUIRE(inspect.begin(s));
  REQUIRE(inspect.refresh(s));
  CHECK(checked(inspect.view(s)).volume == approximately(4800));
  CHECK(checked(inspect.view(s)).known_mass->mass == approximately(.0192));
  inspect.cancel();
  GuiMaterialController editor;
  REQUIRE(editor.begin(s));
  REQUIRE(editor.stage_assignment(s, s.part_document()->id(), {},
                                  resources::key("synthetic.material", "r2")));
  REQUIRE(editor.apply(s, "Assign material to expression-driven Part"));
  const auto* parameter = s.part_document()->find_parameter(ParameterId("computed"));
  REQUIRE(parameter);
  CHECK(parameter->value().millimeters() == approximately(16));
  CHECK(parameter->formula() == std::optional<std::string>("depth * 2"));
  CHECK(geometry::OcctShapeView::get(*s.part_shape_cache()->find_body_shape(
            BodyId("body.computed"))) == geometry::OcctShapeView::get(shape));
  REQUIRE(inspect.begin(s));
  REQUIRE(inspect.refresh(s));
  CHECK(inspect.geometry_reused());
  const auto view = checked(inspect.view(s));
  CHECK(view.volume == approximately(4800));
  CHECK(view.known_mass->mass == approximately(.0288));
  CHECK(view.known_mass->centroid.z == approximately(20.0 / 3));
}

TEST_CASE("Physical controller converts complete box units and clipboard without authored changes",
          "[gui][parity-b151][physical-properties-controller]") {
  auto s = session(fixture::part());
  const auto before = source(s), label = std::string(s.undo_label());
  const auto revision = s.model_revision();
  const auto dirty = s.dirty();
  GuiPhysicalPropertiesController inspect;
  REQUIRE(inspect.begin(s));
  CHECK_FALSE(inspect.view(s));
  REQUIRE(inspect.refresh(s));
  CHECK_FALSE(inspect.geometry_reused());
  REQUIRE(inspect.set_units({GuiPhysicalLengthUnit::Centimeter, GuiPhysicalMassUnit::Gram}));
  const auto v = checked(inspect.view(s));
  CHECK(v.area == approximately(8.8));
  CHECK(v.volume == approximately(1.6));
  REQUIRE(v.known_mass);
  CHECK(v.known_mass->mass == approximately(6.4));
  CHECK(v.known_mass->centroid.z == approximately(.4));
  CHECK(v.known_mass->central_inertia[0] == approximately(.8746666666666667));
  CHECK(v.volume_unit == "cm³");
  CHECK(v.inertia_unit == "g·cm²");
  CHECK(v.coordinate_frame == "Part coordinates");
  REQUIRE(v.materials.size() == 1);
  REQUIRE(v.materials.front().density);
  CHECK(*v.materials.front().density == approximately(4));
  CHECK(v.density_unit == "g/cm³");
  const auto text = checked(inspect.clipboard_text(s));
  CHECK(text.find("Mass: 6.4 g") != std::string::npos);
  CHECK(text.find("Solid volume: 1.6 cm³") != std::string::npos);
  CHECK(text.find("Principal direction 1 (unitless)") != std::string::npos);
  CHECK(text.find("Requested relative integration tolerance: 1e-09") != std::string::npos);
  REQUIRE(inspect.set_units({GuiPhysicalLengthUnit::Meter, GuiPhysicalMassUnit::Kilogram}));
  const auto meters = checked(inspect.view(s));
  CHECK(meters.volume == Catch::Approx(1.6e-6).epsilon(1e-9).margin(1e-15));
  CHECK(meters.known_mass->central_inertia[0] ==
        Catch::Approx(.0064 * 164 / 12 * 1e-6).epsilon(1e-9).margin(1e-15));
  CHECK_FALSE(
      inspect.set_units({static_cast<GuiPhysicalLengthUnit>(99), GuiPhysicalMassUnit::Gram}));
  CHECK(source(s) == before);
  CHECK(s.model_revision() == revision);
  CHECK(s.undo_label() == label);
  CHECK(s.dirty() == dirty);
}

TEST_CASE("Physical scope choices retain exact repeated nested occurrence and Body identities",
          "[gui][parity-b151][physical-properties-controller]") {
  auto s = session(fixture::project());
  GuiPhysicalPropertiesController inspect;
  REQUIRE(inspect.begin(s));
  REQUIRE(inspect.refresh(s));
  const auto whole = checked(inspect.view(s));
  CHECK(whole.volume == approximately(6400));
  REQUIRE(whole.known_mass);
  CHECK(whole.known_mass->mass == approximately(.032));
  CHECK(whole.known_mass->centroid.x == approximately(24));
  CHECK(whole.known_mass->centroid.y == approximately(9));
  CHECK(whole.volume_centroid->x == approximately(22.5));
  CHECK(whole.volume_centroid->y == approximately(7.5));
  CHECK(whole.known_mass->central_inertia[0] == approximately(6.885333333333333));
  CHECK(whole.known_mass->central_inertia[1] == approximately(-1.728));
  const auto choices = checked(inspect.scopes(s));
  REQUIRE(choices.size() == 7);
  CHECK(choices[1].scope.occurrence->component_instance ==
        choices[4].scope.occurrence->component_instance);
  CHECK(choices[1].scope.occurrence->subassembly_path !=
        choices[4].scope.occurrence->subassembly_path);
  REQUIRE(inspect.select_scope(s, choices[4].scope));
  const auto occurrence = checked(inspect.view(s));
  CHECK(occurrence.known_mass->mass == approximately(.016));
  CHECK(occurrence.known_mass->centroid.x == approximately(30));
  CHECK(occurrence.known_mass->centroid.y == approximately(18));
  CHECK(occurrence.scope_label.find("Right") != std::string::npos);
  CHECK(occurrence.coordinate_frame == "Root assembly coordinates");
  auto body = choices[6].scope;
  body.occurrence->transforms_inner_to_outer = {{{10000, 0, 0}, {}}};
  REQUIRE(inspect.select_scope(s, body));
  const auto selected = checked(inspect.view(s));
  CHECK(selected.known_mass->mass == approximately(.0096));
  CHECK(selected.known_mass->centroid.x == approximately(30));
  CHECK(selected.known_mass->centroid.y == approximately(30));
  CHECK_FALSE(inspect.select_scope(s, {BodyId("body.base"), {}}));
  body.occurrence->subassembly_path = {SubassemblyInstanceId("foreign")};
  CHECK_FALSE(inspect.select_scope(s, body));
}

TEST_CASE("Physical refresh reweights material edits and distinguishes partial known centroids",
          "[gui][parity-b151][material-controller][physical-properties-controller]") {
  auto s = session(fixture::part("material.part", true));
  GuiPhysicalPropertiesController inspect;
  REQUIRE(inspect.begin(s));
  REQUIRE(inspect.refresh(s));
  const auto original = checked(inspect.view(s));
  CHECK(original.known_mass->centroid.x == approximately(18));
  inspect.cancel();
  CHECK_FALSE(inspect.clipboard_text(s));
  GuiMaterialController editor;
  REQUIRE(editor.begin(s));
  REQUIRE(editor.stage_assignment(s, s.part_document()->id(), {}, {}));
  REQUIRE(editor.apply(s, "Clear default material"));
  REQUIRE(inspect.begin(s));
  REQUIRE(inspect.refresh(s));
  CHECK(inspect.geometry_reused());
  const auto partial = checked(inspect.view(s));
  CHECK(partial.mass_status == geometry::PhysicalMassStatus::Partial);
  REQUIRE(partial.known_mass);
  CHECK(partial.known_mass->mass == approximately(.0096));
  CHECK(partial.known_mass->centroid.x == approximately(30));
  CHECK(partial.volume_centroid->x == approximately(15));
  CHECK(partial.missing_density_body_count == 1);
  CHECK(partial.unassigned_body_count == 1);
  const auto text = checked(inspect.clipboard_text(s));
  CHECK(text.find("Known-subset Mass:") != std::string::npos);
  CHECK(text.find("Known-subset Principal moments") != std::string::npos);
  REQUIRE(s.undo());
  CHECK_FALSE(inspect.clipboard_text(s));
  REQUIRE(inspect.refresh(s));
  CHECK(checked(inspect.view(s)).mass_status == geometry::PhysicalMassStatus::Complete);
}

TEST_CASE(
    "Physical controller rejects partial foreign and stale contexts and keeps geometry read-only",
    "[gui][parity-b151][physical-properties-controller]") {
  GuiDocumentSession empty;
  GuiPhysicalPropertiesController inspect;
  CHECK_FALSE(inspect.begin(empty));
  auto s = session(fixture::part("material.part", false, true));
  REQUIRE(s.task().begin("foreign"));
  CHECK_FALSE(inspect.begin(s));
  REQUIRE(s.task().cancel());
  REQUIRE(s.begin_part_history_context(1));
  CHECK_FALSE(inspect.begin(s));
  s.cancel_part_workflow();
  REQUIRE(inspect.begin(s));
  REQUIRE(inspect.refresh(s));
  const auto choices = checked(inspect.scopes(s));
  REQUIRE(choices.size() == 2);
  REQUIRE(inspect.select_scope(s, choices[1].scope));
  REQUIRE(s.commit_part_transaction("Suppress creator", [](PartDocument& p) {
    return p.apply_history_intent({{"base"}, {"base"}});
  }));
  CHECK_FALSE(inspect.view(s));
  CHECK_FALSE(inspect.clipboard_text(s));
  CHECK_FALSE(inspect.refresh(s));
  REQUIRE(inspect.select_scope(s, {}));
  const auto no_body = checked(inspect.view(s));
  CHECK(no_body.mass_status == geometry::PhysicalMassStatus::NotApplicable);
  CHECK_FALSE(no_body.known_mass);
  REQUIRE(s.task().begin("foreign"));
  CHECK_FALSE(inspect.view(s));
  REQUIRE(s.task().cancel());
  CHECK_FALSE(inspect.select_scope(s, {BodyId("missing"), {}}));
}

TEST_CASE(
    "Physical empty surface and absent-density scopes expose availability without invented mass",
    "[gui][parity-b151][physical-properties-controller]") {
  SECTION("empty") {
    GuiDocumentSession s;
    REQUIRE(s.create_part(DocumentId("empty"), "Empty"));
    GuiPhysicalPropertiesController inspect;
    REQUIRE(inspect.begin(s));
    REQUIRE(inspect.refresh(s));
    const auto view = checked(inspect.view(s));
    CHECK(view.mass_status == geometry::PhysicalMassStatus::NotApplicable);
    CHECK_FALSE(view.known_mass);
    CHECK_FALSE(view.volume_centroid);
    CHECK(checked(inspect.scopes(s)).size() == 1);
    CHECK(checked(inspect.clipboard_text(s)).find("Solid volume: Not applicable") !=
          std::string::npos);
  }
  SECTION("surface") {
    auto s = session(fixture::surface());
    GuiPhysicalPropertiesController inspect;
    REQUIRE(inspect.begin(s));
    REQUIRE(inspect.refresh(s));
    const auto view = checked(inspect.view(s));
    CHECK(view.area == approximately(50));
    CHECK(view.surface_body_count == 1);
    CHECK(view.mass_status == geometry::PhysicalMassStatus::NotApplicable);
    CHECK_FALSE(view.known_mass);
    CHECK_FALSE(view.volume_centroid);
    CHECK(checked(inspect.clipboard_text(s)).find("\nMass: ") == std::string::npos);
  }
  SECTION("material without density") {
    auto p = fixture::part();
    const auto material = checked(EngineeringMaterial::create(
        resources::key("no.density"), "No density", resources::provenance(), {}));
    REQUIRE(p.add_engineering_resource(material));
    REQUIRE(p.set_default_material(material.key()));
    auto s = session(std::move(p));
    GuiPhysicalPropertiesController inspect;
    REQUIRE(inspect.begin(s));
    REQUIRE(inspect.refresh(s));
    const auto view = checked(inspect.view(s));
    CHECK(view.mass_status == geometry::PhysicalMassStatus::Unavailable);
    CHECK_FALSE(view.known_mass);
    CHECK(view.missing_density_body_count == 1);
    CHECK(view.unassigned_body_count == 0);
    REQUIRE(view.materials.size() == 1);
    CHECK_FALSE(view.materials.front().density);
    CHECK(checked(inspect.clipboard_text(s)).find("density unavailable") != std::string::npos);
  }
}

TEST_CASE("Physical clipboard numbers remain locale independent and Save requires explicit result "
          "refresh",
          "[gui][parity-b151][physical-properties-controller]") {
  struct Comma : std::numpunct<char> {
    char do_decimal_point() const override {
      return ',';
    }
  };
  struct LocaleGuard {
    std::locale old = std::locale();
    ~LocaleGuard() {
      std::locale::global(old);
    }
  } guard;
  auto s = session(fixture::part());
  GuiPhysicalPropertiesController inspect;
  REQUIRE(inspect.begin(s));
  REQUIRE(inspect.refresh(s));
  REQUIRE(inspect.set_units({GuiPhysicalLengthUnit::Centimeter, GuiPhysicalMassUnit::Gram}));
  std::locale::global(std::locale(std::locale::classic(), new Comma));
  CHECK(checked(inspect.clipboard_text(s)).find("Mass: 6.4 g") != std::string::npos);
  QTemporaryDir directory;
  REQUIRE(s.save_as(directory.filePath("part.blcad.json").toStdString()));
  CHECK_FALSE(inspect.clipboard_text(s));
  REQUIRE(inspect.refresh(s));
  CHECK(inspect.geometry_reused());
  CHECK(checked(inspect.view(s)).known_mass->mass == approximately(6.4));
}

TEST_CASE("Reference material intent stays read-only and physical Copy verifies actual STEP bytes",
          "[gui][parity-b151][material-controller][physical-properties-controller][physical-"
          "controller-imports]") {
  QTemporaryDir directory;
  const std::filesystem::path root = directory.path().toStdString();
  const auto p = imported(root, true);
  const auto path = (root / "reference.blcad.json").string();
  REQUIRE(write_part_document_json_file(p, path));
  GuiDocumentSession s;
  REQUIRE(s.open_file(path));
  const auto before = source(s);
  GuiMaterialController editor;
  REQUIRE(editor.begin(s));
  CHECK_FALSE(editor.stage_material(s, resources::material()));
  CHECK_FALSE(editor.stage_assignment(s, p.id(), {}, {}));
  CHECK_FALSE(editor.changed());
  CHECK(checked(editor.apply(s, "Read-only close")) == 0);
  CHECK(source(s) == before);
  GuiPhysicalPropertiesController inspect;
  REQUIRE(inspect.begin(s));
  REQUIRE(inspect.refresh(s));
  const auto view = checked(inspect.view(s));
  CHECK(view.volume == approximately(1600));
  CHECK(view.mass_status == geometry::PhysicalMassStatus::Unavailable);
  CHECK_FALSE(view.known_mass);
  const auto revision = s.model_revision();
  const auto bytes = file((root / "supplier.step").string());
  {
    std::ofstream out(root / "supplier.step", std::ios::binary | std::ios::app);
    out << '\n';
  }
  CHECK(s.model_revision() == revision);
  CHECK_FALSE(inspect.clipboard_text(s));
  CHECK_FALSE(inspect.refresh(s));
  CHECK(source(s) == before);
  {
    std::ofstream out(root / "supplier.step", std::ios::binary | std::ios::trunc);
    out << bytes;
  }
  REQUIRE(inspect.refresh(s));
  CHECK(inspect.geometry_reused());
  CHECK(checked(inspect.view(s)).volume == approximately(1600));
}

TEST_CASE("Failed imported material Apply keeps the committed model and retry reuses its original "
          "geometry",
          "[gui][parity-b151][material-controller][physical-controller-imports]") {
  QTemporaryDir directory;
  const std::filesystem::path root = directory.path().toStdString();
  const auto p = imported(root, false);
  const auto path = (root / "editable.blcad.json").string();
  REQUIRE(write_part_document_json_file(p, path));
  GuiDocumentSession s;
  REQUIRE(s.open_file(path));
  const auto before = source(s), bytes = file((root / "supplier.step").string());
  const auto shape = *s.part_shape_cache()->find_body_shape(BodyId("import.body.0"));
  GuiMaterialController editor;
  REQUIRE(editor.begin(s));
  REQUIRE(editor.stage_assignment(s, p.id(), {}, resources::key("synthetic.material", "r2")));
  {
    std::ofstream out(root / "supplier.step", std::ios::binary | std::ios::app);
    out << '\n';
  }
  CHECK_FALSE(editor.apply(s, "Assign imported material"));
  CHECK(editor.active());
  CHECK(source(s) == before);
  CHECK_FALSE(s.dirty());
  {
    std::ofstream out(root / "supplier.step", std::ios::binary | std::ios::trunc);
    out << bytes;
  }
  REQUIRE(editor.apply(s, "Assign imported material"));
  CHECK(s.dirty());
  CHECK(geometry::OcctShapeView::get(*s.part_shape_cache()->find_body_shape(
            BodyId("import.body.0"))) == geometry::OcctShapeView::get(shape));
  GuiPhysicalPropertiesController inspect;
  REQUIRE(inspect.begin(s));
  REQUIRE(inspect.refresh(s));
  CHECK(checked(inspect.view(s)).known_mass->mass == approximately(.0096));
}
