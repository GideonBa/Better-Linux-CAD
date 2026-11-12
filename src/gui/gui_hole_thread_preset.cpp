#include "blcad/gui/gui_hole_thread_preset.hpp"
#include "blcad/core/part_document_json.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <algorithm>
#include <map>
#include <nlohmann/json.hpp>

namespace blcad::gui {
namespace {
using Json = nlohmann::json;
const FeatureId preset_feature("preset.feature");
Error error(std::string message) {
  return Error::validation("gui.hole_thread.preset", std::move(message));
}
struct Inputs {
  const PartDocument& source;
  PartDocument& target;
  std::string prefix;
  std::map<std::string, ParameterId> copied;
  std::optional<Error> failure;
  ParameterId parameter(const ParameterId& id, std::string role) {
    if (const auto found = copied.find(id.value()); found != copied.end())
      return found->second;
    const auto* value = source.find_parameter(id);
    if (!value) {
      failure = error("A preset dimension is unavailable.");
      return id;
    }
    std::size_t index = copied.size();
    ParameterId next(prefix + ".value." + std::to_string(index));
    while (target.find_parameter(next))
      next = ParameterId(prefix + ".value." + std::to_string(++index));
    std::size_t suffix = 1;
    std::string name;
    do {
      name = "preset_" + role + "_" + std::to_string(suffix++);
    } while (target.find_parameter(std::string_view(name)));
    // Save Values is the explicit snapshot boundary. Document expressions,
    // parameter identities and names are never captured in the value template.
    auto literal = value->type() == ParameterType::Angle
                       ? Parameter::create_angle(next, name, value->value())
                   : value->type() == ParameterType::Count
                       ? Parameter::create_count(next, name, value->value())
                       : Parameter::create_length(next, name, value->value());
    if (!literal)
      failure = literal.error();
    else if (auto added = target.add_parameter(std::move(literal.value())); !added)
      failure = added.error();
    copied.emplace(id.value(), next);
    return next;
  }
  HoleDimension dimension(const HoleDimension& value, std::string role) {
    if (const auto* id = std::get_if<ParameterId>(&value))
      return parameter(*id, std::move(role));
    return value;
  }
  Result<ThreadDefinition> thread(const ThreadDefinition& value) {
    auto profile = value.profile();
    if (auto* custom = std::get_if<ThreadParameterProfile>(&profile)) {
      custom->major_diameter = parameter(custom->major_diameter, "thread_major");
      custom->minor_diameter = parameter(custom->minor_diameter, "thread_minor");
      custom->pitch = parameter(custom->pitch, "thread_pitch");
      custom->included_angle = parameter(custom->included_angle, "thread_angle");
      if (custom->taper_angle)
        custom->taper_angle = parameter(*custom->taper_angle, "thread_taper");
    }
    auto length = parameter(value.length(), "thread_length");
    auto start = value.start();
    if (start)
      start = parameter(*start, "thread_start");
    if (failure)
      return Result<ThreadDefinition>::failure(*failure);
    return ThreadDefinition::create(std::move(profile), length, start, value.handedness(),
                                    value.representation());
  }
};
Result<PartDocument> copy_values(const PartDocument& source, FeatureId from, PartDocument target,
                                 FeatureId into, std::string prefix) {
  const auto* hole = source.find_hole_feature(from);
  const auto* external = source.find_thread_feature(from);
  if ((!hole && !external) || (hole && !target.find_hole_feature(into)) ||
      (external && !target.find_thread_feature(into)) || prefix.empty())
    return Result<PartDocument>::failure(error("Choose a compatible Hole or Thread value preset."));
  const auto resources =
      hole ? hole->standard_dependencies() : external->thread().standard_dependencies();
  for (const auto& key : resources) {
    const auto* resource = source.engineering_resources().find(key);
    if (!resource)
      return Result<PartDocument>::failure(error("A pinned preset revision is unavailable."));
    auto added = target.add_engineering_resource(*resource);
    if (!added)
      return Result<PartDocument>::failure(added.error());
  }
  Inputs inputs{source, target, std::move(prefix), {}, {}};
  if (hole) {
    const auto destination = *target.find_hole_feature(into);
    auto extent = hole->extent();
    if (auto* blind = std::get_if<HoleBlindExtent>(&extent))
      blind->depth = inputs.parameter(blind->depth, "depth");
    else if (auto* symmetric = std::get_if<HoleSymmetricExtent>(&extent))
      symmetric->total_depth = inputs.parameter(symmetric->total_depth, "total_depth");
    else if (auto* face = std::get_if<HoleToFaceExtent>(&extent)) {
      const auto* current = std::get_if<HoleToFaceExtent>(&destination.extent());
      if (!current)
        return Result<PartDocument>::failure(
            error("Choose To Face and its current limit before loading this preset."));
      face->face = current->face;
    } else if (auto* body = std::get_if<HoleUpToBodyExtent>(&extent)) {
      const auto* current = std::get_if<HoleUpToBodyExtent>(&destination.extent());
      if (!current)
        return Result<PartDocument>::failure(
            error("Choose Up to Body and its current limit before loading this preset."));
      body->body = current->body;
    }
    auto tip = hole->tip();
    if (auto* cone = std::get_if<HoleConicalTip>(&tip))
      cone->included_angle = inputs.parameter(cone->included_angle, "tip_angle");
    auto manufacturing = hole->manufacturing();
    std::visit(
        [&](auto& value) {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, HoleCounterbore> || std::is_same_v<T, HoleSpotface>) {
            value.diameter = inputs.dimension(value.diameter, "seat_diameter");
            value.depth = inputs.dimension(value.depth, "seat_depth");
          } else if constexpr (std::is_same_v<T, HoleCountersink> ||
                               std::is_same_v<T, HoleCenter>) {
            value.diameter = inputs.dimension(value.diameter, "seat_diameter");
            value.included_angle = inputs.dimension(value.included_angle, "seat_angle");
          } else if constexpr (std::is_same_v<T, HoleFitted>) {
            value.minimum_diameter = inputs.dimension(value.minimum_diameter, "minimum");
            value.maximum_diameter = inputs.dimension(value.maximum_diameter, "maximum");
          } else if constexpr (std::is_same_v<T, HoleStepped>) {
            const auto* previous = std::get_if<HoleStepped>(&destination.manufacturing());
            for (std::size_t i = 0; i < value.steps.size(); ++i) {
              auto& step = value.steps[i];
              step.id = previous && i < previous->steps.size()
                            ? previous->steps[i].id
                            : inputs.prefix + ".step." + std::to_string(i);
              step.diameter =
                  inputs.dimension(step.diameter, "step_" + std::to_string(i) + "_diameter");
              step.depth = inputs.dimension(step.depth, "step_" + std::to_string(i) + "_depth");
            }
          } else if constexpr (std::is_same_v<T, HoleSlot>) {
            if (const auto* current = std::get_if<HoleSlot>(&destination.manufacturing()))
              value.profile = current->profile;
            else
              inputs.failure =
                  error("Choose the Slot recipe and a current profile before loading this preset.");
          }
        },
        manufacturing);
    auto thread = hole->thread();
    if (thread) {
      auto copied = inputs.thread(*thread);
      if (!copied)
        return Result<PartDocument>::failure(copied.error());
      thread = std::move(copied.value());
    }
    auto diameter = inputs.dimension(hole->diameter(), "bore");
    if (inputs.failure)
      return Result<PartDocument>::failure(*inputs.failure);
    auto copied = HoleFeature::create(destination.id(), destination.name(), hole->kind(),
                                      destination.target_body(), destination.support(),
                                      destination.placements(), diameter, extent, hole->direction(),
                                      tip, manufacturing, thread);
    if (!copied)
      return Result<PartDocument>::failure(copied.error());
    auto updated = target.update_hole_feature(std::move(copied.value()));
    if (!updated)
      return Result<PartDocument>::failure(updated.error());
  } else {
    const auto destination = *target.find_thread_feature(into);
    auto definition = inputs.thread(external->thread());
    if (!definition)
      return Result<PartDocument>::failure(definition.error());
    auto copied = ThreadFeature::create(destination.id(), destination.name(),
                                        destination.target_body(), destination.support(),
                                        std::move(definition.value()), external->direction());
    if (!copied)
      return Result<PartDocument>::failure(copied.error());
    auto updated = target.update_thread_feature(std::move(copied.value()));
    if (!updated)
      return Result<PartDocument>::failure(updated.error());
  }
  return Result<PartDocument>::success(std::move(target));
}

Result<PartDocument> skeleton(const PartDocument& source, FeatureId feature) {
  auto made = PartDocument::create(DocumentId("preset.document"), "Property preset");
  if (!made)
    return made;
  auto& part = made.value();
  auto added = part.add_datum_plane(DatumPlane::xy().value());
  if (!added)
    return Result<PartDocument>::failure(added.error());
  for (const auto* role :
       {"width", "height", "depth", "bore", "minor", "pitch", "length", "angle"}) {
    const double value = std::string_view(role) == "bore"    ? 4
                         : std::string_view(role) == "minor" ? 3
                         : std::string_view(role) == "angle" ? 60
                                                             : 100;
    auto parameter = std::string_view(role) == "angle"
                         ? Parameter::create_angle(ParameterId(std::string("preset.") + role),
                                                   std::string("preset_") + role,
                                                   Quantity::angle_deg(value).value())
                         : Parameter::create_length(ParameterId(std::string("preset.") + role),
                                                    std::string("preset_") + role,
                                                    Quantity::length_mm(value).value());
    if (!parameter)
      return Result<PartDocument>::failure(parameter.error());
    added = part.add_parameter(std::move(parameter.value()));
    if (!added)
      return Result<PartDocument>::failure(added.error());
  }
  auto sketch = Sketch::create(SketchId("preset.sketch"), "Preset stock", DatumPlaneId("datum.xy"));
  if (!sketch)
    return Result<PartDocument>::failure(sketch.error());
  added = sketch.value().add_profile(RectangleProfile::create(ProfileId("preset.profile"),
                                                              ParameterId("preset.width"),
                                                              ParameterId("preset.height"))
                                         .value());
  if (added)
    added = part.add_sketch(std::move(sketch.value()));
  if (!added)
    return Result<PartDocument>::failure(added.error());
  for (const auto* role : {"target", "limit"}) {
    const BodyId body(std::string("preset.") + role);
    auto body_record = Body::create(body, std::string("Preset ") + role, BodyKind::Solid);
    if (!body_record)
      return Result<PartDocument>::failure(body_record.error());
    added = part.add_body(std::move(body_record.value()));
    if (!added)
      return Result<PartDocument>::failure(added.error());
    auto base = Feature::create_additive_extrude(
        FeatureId(std::string("preset.") + role + ".base"), std::string("Preset ") + role,
        SketchId("preset.sketch"), ParameterId("preset.depth"));
    if (!base)
      return Result<PartDocument>::failure(base.error());
    auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, {}, body);
    if (!context)
      return Result<PartDocument>::failure(context.error());
    auto attached = base.value().with_body_result_context(context.value());
    if (!attached)
      return Result<PartDocument>::failure(attached.error());
    added = part.add_feature(std::move(attached.value()));
    if (!added)
      return Result<PartDocument>::failure(added.error());
  }
  if (const auto* hole = source.find_hole_feature(feature)) {
    HoleExtent extent = HoleThroughAllExtent{};
    if (std::holds_alternative<HoleToFaceExtent>(hole->extent())) {
      auto reference = FaceReference::create_planar(
          PartFeatureInputRole::HoleLimitFace,
          SemanticFaceReference::create(FeatureId("preset.limit.base"), SemanticFace::Top).value());
      if (!reference)
        return Result<PartDocument>::failure(reference.error());
      extent = HoleToFaceExtent{reference.value()};
    } else if (std::holds_alternative<HoleUpToBodyExtent>(hole->extent()))
      extent = HoleUpToBodyExtent{BodyId("preset.limit")};
    HoleManufacturing manufacturing;
    if (std::holds_alternative<HoleSlot>(hole->manufacturing())) {
      auto slot = Sketch::create(SketchId("preset.slot"), "Preset slot", DatumPlaneId("datum.xy"));
      if (!slot)
        return Result<PartDocument>::failure(slot.error());
      for (auto line : {LineSegment::create(SketchEntityId("preset.top"), {-2, 1}, {2, 1}),
                        LineSegment::create(SketchEntityId("preset.bottom"), {2, -1}, {-2, -1})}) {
        if (!line)
          return Result<PartDocument>::failure(line.error());
        added = slot.value().add_entity(std::move(line.value()));
        if (!added)
          return Result<PartDocument>::failure(added.error());
      }
      for (auto arc :
           {ArcSegment::create_three_point(SketchEntityId("preset.right"), {2, 1}, {3, 0}, {2, -1}),
            ArcSegment::create_three_point(SketchEntityId("preset.left"), {-2, -1}, {-3, 0},
                                           {-2, 1})}) {
        if (!arc)
          return Result<PartDocument>::failure(arc.error());
        added = slot.value().add_entity(std::move(arc.value()));
        if (!added)
          return Result<PartDocument>::failure(added.error());
      }
      added = slot.value().add_profile(
          ArcClosedProfile::create(ProfileId("preset.slot.profile"),
                                   {SketchEntityId("preset.top"), SketchEntityId("preset.right"),
                                    SketchEntityId("preset.bottom"), SketchEntityId("preset.left")})
              .value());
      if (added)
        added = part.add_sketch(std::move(slot.value()));
      if (!added)
        return Result<PartDocument>::failure(added.error());
      manufacturing = HoleSlot{
          ProfileRegionReference::create(SketchId("preset.slot"), ProfileId("preset.slot.profile"))
              .value()};
    }
    auto support = PlaneReference::create_datum_plane(PartFeatureInputRole::HoleSupportPlane,
                                                      DatumPlaneId("datum.xy"));
    if (!support)
      return Result<PartDocument>::failure(support.error());
    auto placeholder = HoleFeature::create(
        preset_feature, "Preset values", HoleKind::Plain, BodyId("preset.target"), support.value(),
        {{HolePlacementId("preset.position"), Point2{}}}, ParameterId("preset.bore"), extent,
        HoleDirection::SupportNormal, HoleFlatTip{}, manufacturing);
    if (!placeholder)
      return Result<PartDocument>::failure(placeholder.error());
    added = part.add_hole_feature(std::move(placeholder.value()));
  } else if (source.find_thread_feature(feature)) {
    auto support = FaceReference::create_result(
        PartFeatureInputRole::ThreadSupportFace,
        ResultFaceReference::create(FeatureId("preset.target.base"), BodyId("preset.target"),
                                    {{FeatureId("preset.target.base"), "preset.support"}})
            .value());
    if (!support)
      return Result<PartDocument>::failure(support.error());
    auto definition = ThreadDefinition::create(ThreadParameterProfile{"Preset",
                                                                      "Preset",
                                                                      "Preset",
                                                                      ParameterId("preset.bore"),
                                                                      ParameterId("preset.minor"),
                                                                      ParameterId("preset.pitch"),
                                                                      ParameterId("preset.angle"),
                                                                      {}},
                                               ParameterId("preset.length"));
    if (!definition)
      return Result<PartDocument>::failure(definition.error());
    auto placeholder =
        ThreadFeature::create(preset_feature, "Preset values", BodyId("preset.target"),
                              support.value(), definition.value());
    if (!placeholder)
      return Result<PartDocument>::failure(placeholder.error());
    added = part.add_thread_feature(std::move(placeholder.value()));
  } else
    return Result<PartDocument>::failure(error("Choose a Hole or Thread to save as a preset."));
  if (!added)
    return Result<PartDocument>::failure(added.error());
  return made;
}

Result<std::size_t> write(const QString& path, const std::vector<GuiHoleThreadPreset>& presets) {
  Json root{{"schema", "blcad.feature_presets"}, {"version", 1}, {"presets", Json::array()}};
  for (const auto& preset : presets) {
    auto encoded = serialize_part_document_to_json(preset.values);
    if (!encoded)
      return Result<std::size_t>::failure(encoded.error());
    root["presets"].push_back({{"name", preset.name}, {"values", Json::parse(encoded.value())}});
  }
  if (!QDir().mkpath(QFileInfo(path).absolutePath()))
    return Result<std::size_t>::failure(error("The preset directory cannot be created."));
  QSaveFile file(path);
  const auto bytes = QByteArray::fromStdString(root.dump(2));
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
    return Result<std::size_t>::failure(error("The preset file could not be published."));
  return Result<std::size_t>::success(presets.size());
}
} // namespace

bool GuiHoleThreadPreset::hole() const noexcept {
  return values.find_hole_feature(preset_feature) != nullptr;
}
Result<GuiHoleThreadPreset> GuiHoleThreadPresetCommands::capture(const PartDocument& part,
                                                                 FeatureId id, std::string name) {
  name = QString::fromStdString(name).trimmed().toStdString();
  if (name.empty())
    return Result<GuiHoleThreadPreset>::failure(error("Enter a preset name."));
  auto seed = skeleton(part, id);
  if (!seed)
    return Result<GuiHoleThreadPreset>::failure(seed.error());
  auto copied = copy_values(part, id, std::move(seed.value()), preset_feature, "preset.input");
  if (!copied)
    return Result<GuiHoleThreadPreset>::failure(copied.error());
  return Result<GuiHoleThreadPreset>::success({std::move(name), std::move(copied.value())});
}
Result<PartDocument> GuiHoleThreadPresetCommands::apply(const GuiHoleThreadPreset& preset,
                                                        PartDocument part, FeatureId id,
                                                        std::string draft_namespace) {
  return copy_values(preset.values, preset_feature, std::move(part), id,
                     std::move(draft_namespace));
}
Result<std::vector<GuiHoleThreadPreset>> GuiHoleThreadPresetCommands::load(const QString& path) {
  std::vector<GuiHoleThreadPreset> values;
  if (!QFileInfo::exists(path))
    return Result<std::vector<GuiHoleThreadPreset>>::success({});
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return Result<std::vector<GuiHoleThreadPreset>>::failure(
        error("The preset file cannot be read."));
  try {
    const auto root = Json::parse(file.readAll().toStdString());
    if (root.size() != 3 || root.at("schema") != "blcad.feature_presets" ||
        root.at("version") != 1 || !root.at("presets").is_array())
      return Result<std::vector<GuiHoleThreadPreset>>::failure(error("Unsupported preset file."));
    for (const auto& entry : root.at("presets")) {
      if (entry.size() != 2 || !entry.at("name").is_string())
        return Result<std::vector<GuiHoleThreadPreset>>::failure(error("Invalid preset entry."));
      auto part = deserialize_part_document_from_json(entry.at("values").dump());
      const auto name =
          QString::fromStdString(entry.at("name").get<std::string>()).trimmed().toStdString();
      if (!part)
        return Result<std::vector<GuiHoleThreadPreset>>::failure(part.error());
      const bool hole = part.value().find_hole_feature(preset_feature) != nullptr;
      const bool thread = part.value().find_thread_feature(preset_feature) != nullptr;
      if (name.empty() || part.value().id() != DocumentId("preset.document") || hole == thread ||
          std::any_of(values.begin(), values.end(),
                      [&](const auto& old) { return old.name == name && old.hole() == hole; }))
        return Result<std::vector<GuiHoleThreadPreset>>::failure(
            error("Invalid or duplicate preset identity."));
      auto canonical = capture(part.value(), preset_feature, name);
      if (!canonical)
        return Result<std::vector<GuiHoleThreadPreset>>::failure(canonical.error());
      auto expected = serialize_part_document_to_json(canonical.value().values);
      auto actual = serialize_part_document_to_json(part.value());
      if (!expected || !actual || expected.value() != actual.value())
        return Result<std::vector<GuiHoleThreadPreset>>::failure(
            error("The preset contains document-specific or noncanonical bindings."));
      values.push_back(std::move(canonical.value()));
    }
  } catch (const Json::exception&) {
    return Result<std::vector<GuiHoleThreadPreset>>::failure(
        error("The preset file is malformed."));
  }
  return Result<std::vector<GuiHoleThreadPreset>>::success(std::move(values));
}
Result<std::size_t> GuiHoleThreadPresetCommands::save(const QString& path,
                                                      GuiHoleThreadPreset preset) {
  auto canonical = capture(preset.values, preset_feature, preset.name);
  if (!canonical)
    return Result<std::size_t>::failure(canonical.error());
  preset = std::move(canonical.value());
  auto current = load(path);
  if (!current)
    return Result<std::size_t>::failure(current.error());
  std::erase_if(current.value(), [&](const auto& old) {
    return old.name == preset.name && old.hole() == preset.hole();
  });
  current.value().push_back(std::move(preset));
  return write(path, current.value());
}
Result<std::size_t> GuiHoleThreadPresetCommands::remove(const QString& path, std::string_view name,
                                                        bool hole) {
  auto current = load(path);
  if (!current)
    return Result<std::size_t>::failure(current.error());
  const auto removed = std::erase_if(
      current.value(), [&](const auto& old) { return old.name == name && old.hole() == hole; });
  if (!removed)
    return Result<std::size_t>::failure(error("Choose a saved preset to remove."));
  return write(path, current.value());
}
} // namespace blcad::gui
