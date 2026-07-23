#include "blcad/gui/gui_document_browser.hpp"

#include "blcad/core/assembly_document.hpp"
#include "blcad/core/sketch_diagnostics.hpp"

#include <algorithm>
#include <charconv>
#include <concepts>
#include <sstream>

namespace blcad::gui {
namespace {

std::string number(double value) {
  std::ostringstream stream;
  stream.precision(12);
  stream << value;
  return stream.str();
}

std::string quantity_value(const Quantity& value) {
  switch (value.kind()) {
  case QuantityKind::Count:
    return std::to_string(value.count_value());
  case QuantityKind::AngleDeg:
    return number(value.degrees());
  case QuantityKind::LengthMm:
  case QuantityKind::LinearDisplacementMm:
    return number(value.millimeters());
  }
  return {};
}

std::string recompute_status(GuiRecomputeStatus status) {
  switch (status) {
  case GuiRecomputeStatus::Unavailable: return "unavailable";
  case GuiRecomputeStatus::Stale: return "stale";
  case GuiRecomputeStatus::Fresh: return "fresh";
  case GuiRecomputeStatus::Failed: return "failed";
  }
  return "unavailable";
}

std::string diagnostic_severity(GuiDiagnosticSeverity severity) {
  switch (severity) {
  case GuiDiagnosticSeverity::Information: return "information";
  case GuiDiagnosticSeverity::Warning: return "warning";
  case GuiDiagnosticSeverity::Error: return "error";
  }
  return "information";
}

GuiPropertyValue property(std::string key, std::string label, std::string value,
                          GuiPropertyKind kind = GuiPropertyKind::Text, bool editable = false,
                          std::vector<std::string> choices = {},
                          std::string reason = "Derived or unsupported by the current Core contract") {
  return {std::move(key), std::move(label), std::move(value), kind, editable,
          std::move(choices), editable ? std::string{} : std::move(reason)};
}

template <typename T>
std::string object_label(const T& object) {
  if constexpr (requires { { object.name() } -> std::convertible_to<std::string_view>; })
    return object.name();
  return object.id().value();
}

std::string node_status(const PartDocument& part, std::string_view id) {
  const auto* state = part.invalidation_state().find(id);
  return state == nullptr ? "not tracked" : std::string(to_string(state->status));
}

std::string dependencies(const PartDocument& part, std::string_view id, bool consumers) {
  std::vector<std::string> values;
  if (consumers) {
    values = part.dependency_graph().direct_dependents(id);
  } else {
    for (const auto& [dependency, dependent] : part.dependency_graph().dependencies())
      if (dependent == id)
        values.push_back(dependency);
  }
  std::string joined;
  for (const auto& value : values) {
    if (!joined.empty())
      joined += ", ";
    joined += value;
  }
  return joined.empty() ? "—" : joined;
}

template <typename T>
GuiBrowserNode part_node(const PartDocument& part, const T& object, GuiBrowserNodeKind kind,
                         std::optional<GuiSelectionKind> selection = std::nullopt) {
  const std::string id = object.id().value();
  GuiBrowserNode node{kind, id, part.id().value(), object_label(object), selection};
  node.properties.push_back(property("id", "Identifier", id, GuiPropertyKind::Identifier, false,
                                    {}, "Persistent identifiers are generated/read-only"));
  node.properties.push_back(property("name", "Name", object_label(object)));
  node.properties.push_back(property("status", "Recompute status", node_status(part, id),
                                    GuiPropertyKind::Status));
  node.properties.push_back(property("dependencies", "Dependencies", dependencies(part, id, false),
                                    GuiPropertyKind::OrderedInputs));
  node.properties.push_back(property("consumers", "Consumers", dependencies(part, id, true),
                                    GuiPropertyKind::OrderedInputs));
  return node;
}

GuiBrowserNode group(std::string label) {
  return {GuiBrowserNodeKind::Group, {}, {}, std::move(label)};
}

void append_session_diagnostics(GuiBrowserNode& root, const GuiDocumentSession& session) {
  auto diagnostics = group("Session diagnostics");
  std::size_t index = 0;
  for (const auto& diagnostic : session.diagnostics()) {
    GuiBrowserNode node{GuiBrowserNodeKind::Analysis,
                        "diagnostic/" + std::to_string(index++) + "/" + diagnostic.object_id,
                        root.owner_document_id, diagnostic.object_id};
    node.properties = {
        property("object", "Object", diagnostic.object_id, GuiPropertyKind::Reference),
        property("severity", "Severity", diagnostic_severity(diagnostic.severity),
                 GuiPropertyKind::Status),
        property("category", "Category", std::string(to_string(diagnostic.category)),
                 GuiPropertyKind::Enumeration),
        property("message", "Message", diagnostic.message),
    };
    diagnostics.children.push_back(std::move(node));
  }
  root.children.push_back(std::move(diagnostics));
}

template <typename Range>
void append_part_nodes(GuiBrowserNode& parent, const PartDocument& part, const Range& objects,
                       GuiBrowserNodeKind kind,
                       std::optional<GuiSelectionKind> selection = std::nullopt,
                       std::string_view viewport_prefix = {}) {
  for (const auto& object : objects) {
    auto node = part_node(part, object, kind, selection);
    if (!viewport_prefix.empty())
      node.viewport_semantic_id = std::string(viewport_prefix) + object.id().value();
    parent.children.push_back(std::move(node));
  }
}

void append_surface_nodes(GuiBrowserNode& parent, const PartDocument& part) {
  for (const auto& feature : part.surface_features())
    std::visit([&](const auto& concrete) {
      parent.children.push_back(part_node(part, concrete, GuiBrowserNodeKind::Feature));
    }, feature);
}

template <typename T>
void append_sketch_records(GuiBrowserNode& parent, const PartDocument& part, const Sketch& sketch,
                           const std::vector<T>& records, std::string_view family,
                           bool viewport_entity = false) {
  for (const auto& record : records) {
    const std::string id = "sketch/" + sketch.id().value() + "/" + std::string(family) + "/" +
                           record.id().value();
    GuiBrowserNode node{GuiBrowserNodeKind::Sketch, id, part.id().value(), record.id().value(),
                        viewport_entity ? std::optional(GuiSelectionKind::SketchEntity) : std::nullopt};
    if (viewport_entity)
      node.viewport_semantic_id = "sketch/" + sketch.id().value() + "/entity/" + record.id().value();
    node.properties = {
        property("id", "Identifier", record.id().value(), GuiPropertyKind::Identifier, false, {},
                 "Persistent identifiers are read-only"),
        property("owner", "Sketch", sketch.id().value(), GuiPropertyKind::Reference),
        property("family", "Record family", std::string(family), GuiPropertyKind::Enumeration),
    };
    parent.children.push_back(std::move(node));
  }
}

void append_planar_sketches(GuiBrowserNode& parent, const PartDocument& part) {
  for (const auto& sketch : part.sketches()) {
    auto node = part_node(part, sketch, GuiBrowserNodeKind::Sketch);
    node.properties.push_back(property("workplane", "Workplane", sketch.workplane().value(),
                                       GuiPropertyKind::Reference));
    node.properties.push_back(property("profiles", "Profile count",
                                       std::to_string(sketch.profile_count()), GuiPropertyKind::Status));
    append_sketch_records(node, part, sketch, sketch.line_segments(), "line", true);
    append_sketch_records(node, part, sketch, sketch.arc_segments(), "arc", true);
    append_sketch_records(node, part, sketch, sketch.spline_segments(), "spline", true);
    append_sketch_records(node, part, sketch, sketch.projected_points(), "projected-point");
    append_sketch_records(node, part, sketch, sketch.projected_lines(), "projected-line");
    append_sketch_records(node, part, sketch, sketch.reference_generated_lines(), "reference-line");
    append_sketch_records(node, part, sketch, sketch.constraints(), "reference-constraint");
    append_sketch_records(node, part, sketch, sketch.geometric_constraints(), "geometric-constraint");
    append_sketch_records(node, part, sketch, sketch.driving_dimensions(), "dimension");
    append_sketch_records(node, part, sketch, sketch.trim_extend_operations(), "trim-extend");
    append_sketch_records(node, part, sketch, sketch.tangent_continuities(), "tangent");
    append_sketch_records(node, part, sketch, sketch.rectangle_profiles(), "rectangle-profile");
    append_sketch_records(node, part, sketch, sketch.circle_profiles(), "circle-profile");
    append_sketch_records(node, part, sketch, sketch.closed_profiles(), "closed-profile");
    append_sketch_records(node, part, sketch, sketch.arc_closed_profiles(), "arc-profile");
    append_sketch_records(node, part, sketch, sketch.composite_closed_profiles(), "composite-profile");
    append_sketch_records(node, part, sketch, sketch.circular_hole_patterns(), "circular-hole-pattern");

    const auto report = SketchConstraintDiagnostics{}.analyze(sketch);
    for (std::size_t index = 0; index < report.diagnostics().size(); ++index) {
      const auto& diagnostic = report.diagnostics()[index];
      GuiBrowserNode issue{GuiBrowserNodeKind::Analysis,
                           "sketch/" + sketch.id().value() + "/diagnostic/" + std::to_string(index),
                           part.id().value(), diagnostic.target()};
      issue.properties = {
          property("severity", "Severity", std::string(to_string(diagnostic.severity())),
                   GuiPropertyKind::Status),
          property("kind", "Diagnostic", std::string(to_string(diagnostic.kind())),
                   GuiPropertyKind::Enumeration),
          property("target", "Target", diagnostic.target(), GuiPropertyKind::Reference),
          property("message", "Message", diagnostic.message()),
      };
      node.children.push_back(std::move(issue));
    }
    parent.children.push_back(std::move(node));
  }
}

GuiBrowserNode build_part(const PartDocument& part) {
  GuiBrowserNode root{GuiBrowserNodeKind::Document, part.id().value(), part.id().value(), part.name()};
  root.properties = {
      property("id", "Identifier", part.id().value(), GuiPropertyKind::Identifier, false, {},
               "Persistent document identifiers are read-only"),
      property("name", "Name", part.name()),
      property("status", "Recompute status", "document session", GuiPropertyKind::Status),
  };

  auto parameters = group("Parameters");
  for (const auto& parameter : part.parameters()) {
    auto node = part_node(part, parameter, GuiBrowserNodeKind::Parameter);
    node.properties.push_back(property("type", "Type", std::string(to_string(parameter.type())),
                                       GuiPropertyKind::Enumeration));
    node.properties.push_back(property("value", "Value", quantity_value(parameter.value()),
                                       GuiPropertyKind::Quantity, !parameter.is_expression()));
    node.properties.push_back(property("unit", "Unit", std::string(parameter.value().unit())));
    node.properties.push_back(property("scope", "Scope", std::string(to_string(parameter.scope())),
                                       GuiPropertyKind::Enumeration));
    node.properties.push_back(property("formula", "Expression",
                                       parameter.formula().value_or("—"), GuiPropertyKind::Expression,
                                       parameter.is_expression()));
    parameters.children.push_back(std::move(node));
  }
  root.children.push_back(std::move(parameters));

  // Origin geometry (GUI Shell Reset MVP-9R): the seeded principal planes and
  // axes get their own browser folder. The ids match the shell's origin
  // seeding; documents without them see no behavior change.
  const auto is_origin_plane = [](const DatumPlane& plane) {
    const auto& id = plane.id().value();
    return id == "datum.xy" || id == "datum.xz" || id == "datum.yz";
  };
  const auto is_origin_axis = [](const DatumAxis& axis) {
    const auto& id = axis.id().value();
    return id == "datum.axis.x" || id == "datum.axis.y" || id == "datum.axis.z";
  };
  std::vector<DatumPlane> origin_planes;
  std::vector<DatumPlane> other_planes;
  for (const auto& plane : part.datum_planes())
    (is_origin_plane(plane) ? origin_planes : other_planes).push_back(plane);
  std::vector<DatumAxis> origin_axes;
  std::vector<DatumAxis> other_axes;
  for (const auto& axis : part.datum_axes())
    (is_origin_axis(axis) ? origin_axes : other_axes).push_back(axis);

  if (!origin_planes.empty() || !origin_axes.empty()) {
    auto origin = group("Ursprung");
    append_part_nodes(origin, part, origin_planes, GuiBrowserNodeKind::Datum,
                      GuiSelectionKind::Datum, "datum-plane/");
    append_part_nodes(origin, part, origin_axes, GuiBrowserNodeKind::Datum, GuiSelectionKind::Datum,
                      "datum-axis/");
    root.children.push_back(std::move(origin));
  }

  auto datums = group("Datums / Workplanes");
  append_part_nodes(datums, part, other_planes, GuiBrowserNodeKind::Datum, GuiSelectionKind::Datum, "datum-plane/");
  append_part_nodes(datums, part, other_axes, GuiBrowserNodeKind::Datum, GuiSelectionKind::Datum, "datum-axis/");
  append_part_nodes(datums, part, part.derived_workplanes(), GuiBrowserNodeKind::Datum, GuiSelectionKind::Datum);
  root.children.push_back(std::move(datums));

  auto construction = group("Construction");
  append_part_nodes(construction, part, part.construction_points(), GuiBrowserNodeKind::Construction, GuiSelectionKind::Datum);
  append_part_nodes(construction, part, part.construction_lines(), GuiBrowserNodeKind::Construction, GuiSelectionKind::Datum, "construction-line/");
  append_part_nodes(construction, part, part.construction_planes(), GuiBrowserNodeKind::Construction, GuiSelectionKind::Datum, "construction-plane/");
  root.children.push_back(std::move(construction));

  auto sketches = group("Sketches");
  append_planar_sketches(sketches, part);
  append_part_nodes(sketches, part, part.sketches_3d(), GuiBrowserNodeKind::Sketch, GuiSelectionKind::SketchEntity);
  root.children.push_back(std::move(sketches));

  auto paths = group("Paths");
  append_part_nodes(paths, part, part.path_curves(), GuiBrowserNodeKind::Path, GuiSelectionKind::SketchEntity);
  root.children.push_back(std::move(paths));

  auto features = group("Features");
  append_part_nodes(features, part, part.features(), GuiBrowserNodeKind::Feature);
  append_part_nodes(features, part, part.revolve_features(), GuiBrowserNodeKind::Feature);
  append_part_nodes(features, part, part.sweep_features(), GuiBrowserNodeKind::Feature);
  append_part_nodes(features, part, part.loft_features(), GuiBrowserNodeKind::Feature);
  append_surface_nodes(features, part);
  append_part_nodes(features, part, part.linear_pattern_features(), GuiBrowserNodeKind::Feature);
  append_part_nodes(features, part, part.circular_pattern_features(), GuiBrowserNodeKind::Feature);
  append_part_nodes(features, part, part.mirror_features(), GuiBrowserNodeKind::Feature);
  append_part_nodes(features, part, part.fillet_features(), GuiBrowserNodeKind::Feature);
  append_part_nodes(features, part, part.chamfer_features(), GuiBrowserNodeKind::Feature);
  append_part_nodes(features, part, part.shell_features(), GuiBrowserNodeKind::Feature);
  append_part_nodes(features, part, part.draft_features(), GuiBrowserNodeKind::Feature);
  append_part_nodes(features, part, part.body_boolean_features(), GuiBrowserNodeKind::Feature);
  append_part_nodes(features, part, part.body_transforms(), GuiBrowserNodeKind::Feature);
  root.children.push_back(std::move(features));

  auto bodies = group("Bodies");
  for (const auto& body : part.bodies()) {
    auto node = part_node(part, body, GuiBrowserNodeKind::Body, GuiSelectionKind::Body);
    node.viewport_semantic_id = "body/" + body.id().value();
    node.properties.push_back(property("kind", "Kind", std::string(to_string(body.kind())),
                                       GuiPropertyKind::Enumeration));
    node.properties.push_back(property("visibility", "Visible",
                                       body.visibility() == BodyVisibility::Visible ? "true" : "false",
                                       GuiPropertyKind::Boolean, true, {"true", "false"}));
    node.properties.push_back(property("active", "Active body", "not defined",
                                       GuiPropertyKind::Status, false, {},
                                       "The current Core model has no active-body intent"));
    bodies.children.push_back(std::move(node));
  }
  root.children.push_back(std::move(bodies));

  auto analysis = group("Analysis / References");
  append_part_nodes(analysis, part, part.reference_statuses(), GuiBrowserNodeKind::Analysis);
  append_part_nodes(analysis, part, part.reference_remaps(), GuiBrowserNodeKind::Analysis);
  root.children.push_back(std::move(analysis));
  return root;
}

GuiBrowserNode build_assembly(const AssemblyDocument& assembly, std::string label = {}) {
  GuiBrowserNode root{GuiBrowserNodeKind::Assembly, assembly.id().value(), assembly.id().value(),
                      label.empty() ? assembly.name() : std::move(label)};
  root.properties = {property("id", "Identifier", assembly.id().value(), GuiPropertyKind::Identifier,
                              false, {}, "Persistent identifiers are read-only"),
                     property("name", "Name", assembly.name())};

  auto parameters = group("Parameters");
  for (const auto& parameter : assembly.parameters()) {
    GuiBrowserNode node{GuiBrowserNodeKind::Parameter, parameter.id().value(), assembly.id().value(), parameter.name()};
    node.properties = {
        property("id", "Identifier", parameter.id().value(), GuiPropertyKind::Identifier, false, {}, "Persistent identifiers are read-only"),
        property("name", "Name", parameter.name()),
        property("type", "Type", std::string(to_string(parameter.type())), GuiPropertyKind::Enumeration),
        property("value", "Value", quantity_value(parameter.value()), GuiPropertyKind::Quantity, !parameter.is_expression()),
        property("unit", "Unit", std::string(parameter.value().unit())),
        property("formula", "Expression", parameter.formula().value_or("—"), GuiPropertyKind::Expression),
    };
    parameters.children.push_back(std::move(node));
  }
  root.children.push_back(std::move(parameters));

  auto components = group("Components");
  for (const auto& component : assembly.component_instances()) {
    GuiBrowserNode node{GuiBrowserNodeKind::Component, component.id().value(), assembly.id().value(),
                        component.name(), GuiSelectionKind::Component};
    node.viewport_semantic_id = "component/" + component.id().value();
    node.properties = {
        property("id", "Identifier", component.id().value(), GuiPropertyKind::Identifier, false, {}, "Persistent identifiers are read-only"),
        property("name", "Name", component.name()),
        property("reference", "Part document", component.referenced_part_document().value(), GuiPropertyKind::Reference),
        property("visibility", "Visible", component.visibility() == ComponentVisibility::Visible ? "true" : "false", GuiPropertyKind::Boolean, true, {"true", "false"}),
        property("suppression", "Suppression", std::string(to_string(component.suppression_state())), GuiPropertyKind::Enumeration, true, {"active", "suppressed"}),
        property("grounding", "Grounding", std::string(to_string(component.grounding_state())), GuiPropertyKind::Enumeration, true, {"free", "grounded"}),
    };
    components.children.push_back(std::move(node));
  }
  root.children.push_back(std::move(components));

  auto subassemblies = group("Subassemblies");
  for (const auto& occurrence : assembly.subassembly_instances()) {
    GuiBrowserNode node{GuiBrowserNodeKind::Subassembly, occurrence.id().value(), assembly.id().value(),
                        occurrence.name(), GuiSelectionKind::Component};
    node.properties = {
        property("id", "Identifier", occurrence.id().value(), GuiPropertyKind::Identifier, false, {}, "Persistent identifiers are read-only"),
        property("name", "Name", occurrence.name()),
        property("reference", "Assembly document", occurrence.referenced_assembly_document().value(), GuiPropertyKind::Reference),
        property("visibility", "Visible", occurrence.visibility() == ComponentVisibility::Visible ? "true" : "false", GuiPropertyKind::Boolean, true, {"true", "false"}),
        property("suppression", "Suppression", std::string(to_string(occurrence.suppression_state())), GuiPropertyKind::Enumeration, true, {"active", "suppressed"}),
    };
    subassemblies.children.push_back(std::move(node));
  }
  root.children.push_back(std::move(subassemblies));

  auto constraints = group("Constraints");
  for (const auto& constraint : assembly.constraints()) {
    GuiBrowserNode node{GuiBrowserNodeKind::Constraint, constraint.id().value(), assembly.id().value(), constraint.id().value(), GuiSelectionKind::AssemblyTarget};
    node.properties = {property("id", "Identifier", constraint.id().value(), GuiPropertyKind::Identifier, false, {}, "Persistent identifiers are read-only"),
                       property("kind", "Type", std::string(to_string(constraint.type())), GuiPropertyKind::Enumeration),
                       property("status", "State", std::string(to_string(constraint.state())), GuiPropertyKind::Status)};
    constraints.children.push_back(std::move(node));
  }
  root.children.push_back(std::move(constraints));

  auto joints = group("Joints");
  for (const auto& joint : assembly.joints()) {
    GuiBrowserNode node{GuiBrowserNodeKind::Joint, joint.id().value(), assembly.id().value(), joint.id().value(), GuiSelectionKind::AssemblyTarget};
    node.properties = {property("id", "Identifier", joint.id().value(), GuiPropertyKind::Identifier, false, {}, "Persistent identifiers are read-only"),
                       property("kind", "Type", std::string(to_string(joint.type())), GuiPropertyKind::Enumeration),
                       property("status", "State", std::string(to_string(joint.state())), GuiPropertyKind::Status)};
    joints.children.push_back(std::move(node));
  }
  root.children.push_back(std::move(joints));
  return root;
}

const GuiBrowserNode* find_node(const GuiBrowserNode& node, std::string_view id) {
  if (!id.empty() && (node.semantic_id == id || node.viewport_semantic_id == id))
    return &node;
  for (const auto& child : node.children)
    if (const auto* found = find_node(child, id))
      return found;
  return nullptr;
}

Result<double> parse_number(std::string_view value, std::string_view object_id) {
  double parsed{};
  const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (error != std::errc{} || end != value.data() + value.size())
    return Result<double>::failure(Error::validation(std::string(object_id), "value must be a finite number"));
  return Result<double>::success(parsed);
}

Result<Quantity> edited_quantity(const Parameter& parameter, std::string_view text) {
  const auto parsed = parse_number(text, parameter.id().value());
  if (parsed.has_error())
    return Result<Quantity>::failure(parsed.error());
  switch (parameter.type()) {
  case ParameterType::Length: return Quantity::length_mm(parsed.value(), parameter.id().value());
  case ParameterType::Count: return Quantity::count(parsed.value(), parameter.id().value());
  case ParameterType::Angle: return Quantity::angle_deg(parsed.value(), parameter.id().value());
  }
  return Result<Quantity>::failure(Error::internal(parameter.id().value(), "unsupported parameter type"));
}

Result<std::size_t> unsupported(std::string_view id, std::string_view key) {
  return Result<std::size_t>::failure(Error::validation(std::string(id), "property '" + std::string(key) + "' is read-only"));
}

} // namespace

GuiDocumentBrowser GuiDocumentBrowser::build(const GuiDocumentSession& session) {
  GuiDocumentBrowser browser;
  if (const auto* part = session.part_document()) {
    browser.roots_.push_back(build_part(*part));
    for (auto& field : browser.roots_.back().properties)
      if (field.key == "status")
        field.value = recompute_status(session.recompute_status());
    append_session_diagnostics(browser.roots_.back(), session);
  } else if (const auto* project = session.project()) {
    GuiBrowserNode project_root{GuiBrowserNodeKind::Document, project->id().value(), project->id().value(), project->name()};
    project_root.properties = {property("id", "Identifier", project->id().value(), GuiPropertyKind::Identifier, false, {}, "Persistent identifiers are read-only"),
                               property("name", "Name", project->name()),
                               property("status", "Recompute status", recompute_status(session.recompute_status()), GuiPropertyKind::Status)};
    project_root.children.push_back(build_assembly(project->assembly(), "Root Assembly"));
    auto children = group("Assembly documents");
    for (const auto& assembly : project->child_assembly_documents())
      children.children.push_back(build_assembly(assembly));
    project_root.children.push_back(std::move(children));
    auto parts = group("Part documents");
    for (const auto& part : project->part_documents())
      parts.children.push_back(build_part(part));
    project_root.children.push_back(std::move(parts));
    auto analysis = group("Cross-hierarchy analysis");
    for (const auto& constraint : project->cross_hierarchy_constraints())
      analysis.children.push_back({GuiBrowserNodeKind::Constraint, constraint.id().value(), project->id().value(), constraint.id().value(), GuiSelectionKind::AssemblyTarget});
    for (const auto& joint : project->cross_hierarchy_joints())
      analysis.children.push_back({GuiBrowserNodeKind::Joint, joint.id().value(), project->id().value(), joint.id().value(), GuiSelectionKind::AssemblyTarget});
    project_root.children.push_back(std::move(analysis));
    append_session_diagnostics(project_root, session);
    browser.roots_.push_back(std::move(project_root));
  }
  return browser;
}

const std::vector<GuiBrowserNode>& GuiDocumentBrowser::roots() const noexcept { return roots_; }

const GuiBrowserNode* GuiDocumentBrowser::find(std::string_view semantic_id) const noexcept {
  for (const auto& root : roots_)
    if (const auto* found = find_node(root, semantic_id))
      return found;
  return nullptr;
}

Result<std::size_t> edit_browser_property(GuiDocumentSession& session,
                                          const GuiBrowserNode& node, std::string_view key,
                                          std::string_view value, bool commit) {
  const bool bool_value = value == "true";
  if ((key == "visibility") && value != "true" && value != "false")
    return Result<std::size_t>::failure(Error::validation(node.semantic_id, "visibility must be true or false"));

  if (node.kind == GuiBrowserNodeKind::Body && key == "visibility") {
    const auto mutation = [id = BodyId(node.semantic_id), bool_value](PartDocument& part) {
      return part.set_body_visibility(id, bool_value ? BodyVisibility::Visible : BodyVisibility::Hidden);
    };
    if (!commit)
      return Result<std::size_t>::success(1);
    if (session.part_document())
      return session.commit_part_transaction("Set body visibility", mutation);
    return session.commit_project_transaction("Set body visibility", [owner = DocumentId(node.owner_document_id), mutation](Project& project) {
      auto* part = project.find_part_document(owner);
      return part == nullptr ? Result<std::size_t>::failure(Error::validation(owner.value(), "part document not found")) : mutation(*part);
    });
  }

  if (node.kind == GuiBrowserNodeKind::Parameter && key == "value") {
    if (const auto* part = session.part_document()) {
      const auto* parameter = part->find_parameter(ParameterId(node.semantic_id));
      if (!parameter) return unsupported(node.semantic_id, key);
      auto quantity = edited_quantity(*parameter, value);
      if (quantity.has_error()) return Result<std::size_t>::failure(quantity.error());
      if (!commit) return Result<std::size_t>::success(1);
      return session.commit_part_transaction("Edit parameter", [id = ParameterId(node.semantic_id), quantity = quantity.value()](PartDocument& document) {
        auto changed = document.set_parameter_value(id, quantity);
        return changed.has_error() ? Result<std::size_t>::failure(changed.error()) : Result<std::size_t>::success(changed.value().size());
      });
    }
    const auto* project = session.project();
    if (!project) return unsupported(node.semantic_id, key);
    const auto* assembly = project->find_assembly_document(DocumentId(node.owner_document_id));
    if (assembly) {
      const auto* parameter = assembly->find_parameter(ParameterId(node.semantic_id));
      if (!parameter) return unsupported(node.semantic_id, key);
      auto quantity = edited_quantity(*parameter, value);
      if (quantity.has_error()) return Result<std::size_t>::failure(quantity.error());
      if (!commit) return Result<std::size_t>::success(1);
      return session.commit_project_transaction("Edit assembly parameter", [owner = DocumentId(node.owner_document_id), id = ParameterId(node.semantic_id), quantity = quantity.value()](Project& candidate) {
        auto* target = candidate.find_assembly_document(owner);
        return target == nullptr ? Result<std::size_t>::failure(Error::validation(owner.value(), "assembly document not found")) : target->set_parameter_value(id, quantity);
      });
    }
    const auto* part = project->find_part_document(DocumentId(node.owner_document_id));
    if (!part) return unsupported(node.semantic_id, key);
    const auto* parameter = part->find_parameter(ParameterId(node.semantic_id));
    if (!parameter) return unsupported(node.semantic_id, key);
    auto quantity = edited_quantity(*parameter, value);
    if (quantity.has_error()) return Result<std::size_t>::failure(quantity.error());
    if (!commit) return Result<std::size_t>::success(1);
    return session.commit_project_transaction("Edit part parameter", [owner = DocumentId(node.owner_document_id), id = ParameterId(node.semantic_id), quantity = quantity.value()](Project& candidate) {
      auto* target = candidate.find_part_document(owner);
      if (!target) return Result<std::size_t>::failure(Error::validation(owner.value(), "part document not found"));
      auto changed = target->set_parameter_value(id, quantity);
      return changed.has_error() ? Result<std::size_t>::failure(changed.error()) : Result<std::size_t>::success(changed.value().size());
    });
  }

  if (node.kind == GuiBrowserNodeKind::Parameter && key == "formula") {
    const auto mutation = [id = ParameterId(node.semantic_id), formula = std::string(value)](PartDocument& part) {
      auto changed = part.set_parameter_formula(id, formula);
      return changed.has_error() ? Result<std::size_t>::failure(changed.error())
                                 : Result<std::size_t>::success(changed.value().size());
    };
    if (session.part_document()) {
      if (!commit) {
        PartDocument candidate = *session.part_document();
        return mutation(candidate);
      }
      return session.commit_part_transaction("Edit parameter expression", mutation);
    }
    const auto* project = session.project();
    const auto* part = project ? project->find_part_document(DocumentId(node.owner_document_id)) : nullptr;
    if (!part)
      return unsupported(node.semantic_id, key);
    if (!commit) {
      PartDocument candidate = *part;
      return mutation(candidate);
    }
    return session.commit_project_transaction("Edit parameter expression",
        [owner = DocumentId(node.owner_document_id), mutation](Project& candidate) {
          auto* target = candidate.find_part_document(owner);
          return target == nullptr
                     ? Result<std::size_t>::failure(Error::validation(owner.value(), "part document not found"))
                     : mutation(*target);
        });
  }

  if ((node.kind == GuiBrowserNodeKind::Component || node.kind == GuiBrowserNodeKind::Subassembly) &&
      (key == "visibility" || key == "suppression" || key == "grounding")) {
    if (key == "suppression" && value != "active" && value != "suppressed")
      return Result<std::size_t>::failure(Error::validation(node.semantic_id, "suppression must be active or suppressed"));
    if (key == "grounding" && value != "free" && value != "grounded")
      return Result<std::size_t>::failure(Error::validation(node.semantic_id, "grounding must be free or grounded"));
    if (node.kind == GuiBrowserNodeKind::Subassembly && key == "grounding")
      return unsupported(node.semantic_id, key);
    if (!commit) return Result<std::size_t>::success(1);
    return session.commit_project_transaction("Edit assembly occurrence", [node, key = std::string(key), value = std::string(value), bool_value](Project& project) {
      auto* assembly = project.find_assembly_document(DocumentId(node.owner_document_id));
      if (!assembly) return Result<std::size_t>::failure(Error::validation(node.owner_document_id, "assembly document not found"));
      if (node.kind == GuiBrowserNodeKind::Component) {
        const ComponentInstanceId id(node.semantic_id);
        if (key == "visibility") return assembly->set_component_instance_visibility(id, bool_value ? ComponentVisibility::Visible : ComponentVisibility::Hidden);
        if (key == "suppression") return assembly->set_component_instance_suppression_state(id, value == "active" ? ComponentSuppressionState::Active : ComponentSuppressionState::Suppressed);
        return assembly->set_component_instance_grounding_state(id, value == "free" ? ComponentGroundingState::Free : ComponentGroundingState::Grounded);
      }
      const SubassemblyInstanceId id(node.semantic_id);
      if (key == "visibility") return assembly->set_subassembly_instance_visibility(id, bool_value ? ComponentVisibility::Visible : ComponentVisibility::Hidden);
      return assembly->set_subassembly_instance_suppression_state(id, value == "active" ? ComponentSuppressionState::Active : ComponentSuppressionState::Suppressed);
    });
  }
  return unsupported(node.semantic_id, key);
}

} // namespace blcad::gui
