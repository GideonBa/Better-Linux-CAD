#pragma once

#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project.hpp"
#include "engineering_resource_fixture.hpp"
#include "part_history_fixture.hpp"

#include <nlohmann/json.hpp>

namespace blcad::test::material_physical_fixture {
namespace resources = engineering_fixture;
namespace history = history_fixture;
using history::accept;
using history::checked;
inline PartDocument part(std::string id = "material.part", bool multiple = false,
                         bool authored = false) {
  auto data =
      nlohmann::json::parse(checked(serialize_part_document_to_json(history::native_base())));
  data["document"]["id"] = id;
  auto p = checked(deserialize_part_document_from_json(data.dump()));
  if (multiple) {
    const auto context = history::new_body(p, "second");
    auto feature = checked(Feature::create_additive_extrude(
        FeatureId("second"), "Second", SketchId("profile"), ParameterId("depth")));
    accept(p.add_feature(checked(feature.with_body_result_context(context))));
    accept(p.add_body_transform(checked(BodyTransform::create_translate(
        BodyTransformId("shift"), BodyId("body.second"), BodyTransformCoordinateSpace::World,
        std::nullopt, {30, 0, 0}, false, false))));
  }
  resources::annotate(p);
  if (multiple)
    accept(p.set_body_material(BodyId("body.second"), resources::key("synthetic.material", "r2")));
  if (authored)
    accept(p.apply_history_intent({multiple ? std::vector<std::string>{"base", "second", "shift"}
                                            : std::vector<std::string>{"base"},
                                   {}}));
  return p;
}
inline Project project(bool unused_member = false) {
  auto p = part("material.part", true, true);
  auto root = checked(AssemblyDocument::create(DocumentId("material.root"), "Root"));
  accept(root.add_member_part(p.id()));
  auto project = checked(Project::create(DocumentId("material.project"), "Materials", root));
  auto child = checked(AssemblyDocument::create(DocumentId("material.child"), "Child"));
  accept(child.add_member_part(p.id()));
  accept(child.add_component_instance(
      checked(ComponentInstance::create(ComponentInstanceId("inner"), "Inner", p.id()))));
  accept(project.add_child_assembly_document(child));
  accept(project.assembly().add_subassembly_instance(
      checked(SubassemblyInstance::create(SubassemblyInstanceId("left"), "Left", child.id()))));
  accept(project.assembly().add_subassembly_instance(checked(SubassemblyInstance::create(
      SubassemblyInstanceId("right"), "Right", child.id(), ComponentVisibility::Hidden,
      ComponentSuppressionState::Active, {{30, 0, 0}, {0, 0, 90}}))));
  accept(project.add_part_document(std::move(p)));
  if (unused_member) {
    auto unused = part("unselected", false, true);
    accept(project.assembly().add_member_part(unused.id()));
    accept(project.add_part_document(std::move(unused)));
  }
  return project;
}
inline PartDocument surface() {
  auto p = part();
  accept(p.remove_feature(FeatureId("base")));
  accept(p.remove_body(BodyId("body.base")));
  const auto path = [&](const char* id, double y) {
    auto sketch = checked(
        Sketch::create(SketchId(std::string("sketch.") + id), id, DatumPlaneId("datum.xy")));
    accept(
        sketch.add_entity(checked(LineSegment::create(SketchEntityId("line"), {0, y}, {10, y}))));
    const auto segment = checked(PathSegmentReference::create_planar(
        sketch.id(), SketchEntityId("line"), PlanarPathCurveKind::Line));
    accept(p.add_sketch(std::move(sketch)));
    accept(p.add_path_curve(checked(PathCurve::create(
        PathCurveId(id), id, {segment}, PathClosure::Open, PathOrientationRule::MinimumTwist))));
    return checked(BoundaryCurveReference::create_path_curve(PathCurveId(id)));
  };
  const auto first = path("one", 0), second = path("two", 5);
  accept(p.add_body(checked(Body::create(BodyId("surface"), "Surface", BodyKind::Surface))));
  accept(p.add_surface_feature(checked(BoundarySurfaceFeature::create(
      FeatureId("surface"), "Surface", {first, second}, BodyId("surface")))));
  return p;
}
} // namespace blcad::test::material_physical_fixture
