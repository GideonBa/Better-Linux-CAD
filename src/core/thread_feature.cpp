#include "blcad/core/thread_feature.hpp"

#include "blcad/core/part_document.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace blcad {
namespace {
bool nonblank(const std::string& value) {
  return value.find_first_not_of(" \t\r\n") != std::string::npos;
}
bool valid_profile(const ThreadProfile& profile) {
  if (const auto* custom = std::get_if<ThreadParameterProfile>(&profile))
    return nonblank(custom->system) && nonblank(custom->designation) &&
           nonblank(custom->tolerance) && !custom->major_diameter.empty() &&
           !custom->minor_diameter.empty() && !custom->pitch.empty() &&
           !custom->included_angle.empty() &&
           (!custom->taper_angle || !custom->taper_angle->empty());
  const auto& selected = std::get<ThreadStandardProfile>(profile);
  const auto& c = selected.columns;
  return nonblank(selected.row) && nonblank(c.system) && nonblank(c.designation) &&
         nonblank(c.tolerance) && nonblank(c.major_diameter) && nonblank(c.minor_diameter) &&
         nonblank(c.pitch) && nonblank(c.included_angle) &&
         (!c.taper_angle || nonblank(*c.taper_angle));
}
} // namespace

std::string_view to_string(ThreadHandedness value) noexcept {
  switch (value) {
  case ThreadHandedness::Right:
    return "right";
  case ThreadHandedness::Left:
    return "left";
  }
  return "unknown";
}
std::string_view to_string(ThreadRepresentation value) noexcept {
  switch (value) {
  case ThreadRepresentation::Cosmetic:
    return "cosmetic";
  case ThreadRepresentation::Modeled:
    return "modeled";
  }
  return "unknown";
}
std::string_view to_string(ThreadDirection value) noexcept {
  switch (value) {
  case ThreadDirection::AlongSupportAxis:
    return "along_support_axis";
  case ThreadDirection::OppositeSupportAxis:
    return "opposite_support_axis";
  }
  return "unknown";
}

ThreadDefinition::ThreadDefinition(ThreadProfile profile, ParameterId length,
                                   std::optional<ParameterId> start, ThreadHandedness handedness,
                                   ThreadRepresentation representation)
    : profile_(std::move(profile)), length_(std::move(length)), start_(std::move(start)),
      handedness_(handedness), representation_(representation) {}

Result<ThreadDefinition> ThreadDefinition::create(ThreadProfile profile, ParameterId length,
                                                  std::optional<ParameterId> start,
                                                  ThreadHandedness handedness,
                                                  ThreadRepresentation representation) {
  if (!valid_profile(profile) || length.empty() || (start && start->empty()) ||
      (handedness != ThreadHandedness::Right && handedness != ThreadHandedness::Left) ||
      (representation != ThreadRepresentation::Cosmetic &&
       representation != ThreadRepresentation::Modeled))
    return Result<ThreadDefinition>::failure(Error::validation(
        "thread", "Thread requires complete profile selectors, length and valid modes"));
  return Result<ThreadDefinition>::success(ThreadDefinition(
      std::move(profile), std::move(length), std::move(start), handedness, representation));
}

std::vector<ParameterId> ThreadDefinition::parameter_dependencies() const {
  std::vector<ParameterId> result;
  const auto append = [&](const ParameterId& id) {
    if (std::find(result.begin(), result.end(), id) == result.end())
      result.push_back(id);
  };
  if (const auto* p = std::get_if<ThreadParameterProfile>(&profile_)) {
    for (const auto& id : {p->major_diameter, p->minor_diameter, p->pitch, p->included_angle})
      append(id);
    if (p->taper_angle)
      append(*p->taper_angle);
  }
  if (start_)
    append(*start_);
  append(length_);
  return result;
}
std::vector<EngineeringResourceKey> ThreadDefinition::standard_dependencies() const {
  if (const auto* p = std::get_if<ThreadStandardProfile>(&profile_))
    return {p->table};
  return {};
}
Result<ThreadDefinition>
ThreadDefinition::remap_standard_revision(const EngineeringResourceKey& source,
                                          const EngineeringResourceKey& next) const {
  auto profile = profile_;
  if (auto* selected = std::get_if<ThreadStandardProfile>(&profile);
      selected && selected->table == source)
    selected->table = next;
  return create(std::move(profile), length_, start_, handedness_, representation_);
}

Result<ResolvedThreadDefinition> resolve_thread_definition(const PartDocument& part,
                                                           const ThreadDefinition& definition,
                                                           std::string_view context) {
  const auto fail = [&](std::string message) {
    return Result<ResolvedThreadDefinition>::failure(
        Error::validation(std::string(context), std::move(message)));
  };
  ResolvedThreadDefinition result;
  result.handedness = definition.handedness();
  result.representation = definition.representation();
  const auto parameter = [&](const ParameterId& id, bool angle) -> std::optional<double> {
    const auto* value = part.find_parameter(id);
    if (!value || value->type() != (angle ? ParameterType::Angle : ParameterType::Length))
      return {};
    const double v = angle ? value->value().degrees() : value->value().millimeters();
    if (!std::isfinite(v) || (!angle && v <= 0))
      return {};
    return v;
  };
  auto length = parameter(definition.length(), false);
  auto start = definition.start() ? parameter(*definition.start(), false) : std::optional(0.0);
  if (!length || !start || !std::isfinite(*length + *start))
    return fail("Thread start and length require finite Length inputs with a finite end position");
  result.start_mm = *start;
  result.length_mm = *length;
  if (const auto* p = std::get_if<ThreadParameterProfile>(&definition.profile())) {
    result.system = p->system;
    result.designation = p->designation;
    result.tolerance = p->tolerance;
    const auto major = parameter(p->major_diameter, false),
               minor = parameter(p->minor_diameter, false), pitch = parameter(p->pitch, false),
               angle = parameter(p->included_angle, true);
    const auto taper = p->taper_angle ? parameter(*p->taper_angle, true) : std::optional(0.0);
    if (!major || !minor || !pitch || !angle || !taper)
      return fail("Thread profile requires existing Length dimensions and Angle parameters");
    result.major_diameter_mm = *major;
    result.minor_diameter_mm = *minor;
    result.pitch_mm = *pitch;
    result.included_angle_deg = *angle;
    result.taper_angle_deg = *taper;
    if (p->taper_angle && *taper == 0)
      return fail("Explicit thread taper must be nonzero");
  } else {
    const auto& selected = std::get<ThreadStandardProfile>(definition.profile());
    const auto* table = part.engineering_resources().find_table(selected.table);
    const auto* row = table ? table->find_row(selected.row) : nullptr;
    if (!row)
      return fail("Pinned Thread table revision or row is unavailable");
    result.standard = selected.table;
    const auto cell = [&](const std::string& key, EngineeringTableValueKind kind,
                          const std::string& field) -> const EngineeringTableCell* {
      const auto column = std::find_if(table->columns().begin(), table->columns().end(),
                                       [&](const auto& c) { return c.key == key; });
      if (column == table->columns().end() || column->kind != kind)
        return nullptr;
      const auto value = std::find_if(row->cells.begin(), row->cells.end(),
                                      [&](const auto& c) { return c.column == key; });
      if (value == row->cells.end())
        return nullptr;
      result.provenance.push_back({field, value->provenance});
      return &*value;
    };
    const auto text = [&](const std::string& key, const std::string& field, std::string& output) {
      const auto* c = cell(key, EngineeringTableValueKind::Text, field);
      const auto* v = c ? std::get_if<std::string>(&c->value) : nullptr;
      if (!v || !nonblank(*v))
        return false;
      output = *v;
      return true;
    };
    const auto number = [&](const std::string& key, const std::string& field, bool angle,
                            double& output) {
      const auto* c = cell(
          key, angle ? EngineeringTableValueKind::Angle : EngineeringTableValueKind::Length, field);
      const auto* v = c ? std::get_if<Quantity>(&c->value) : nullptr;
      if (!v || v->kind() != (angle ? QuantityKind::AngleDeg : QuantityKind::LengthMm))
        return false;
      output = angle ? v->degrees() : v->millimeters();
      return std::isfinite(output) && (angle || output > 0);
    };
    const auto& c = selected.columns;
    if (!text(c.system, "system", result.system) ||
        !text(c.designation, "designation", result.designation) ||
        !text(c.tolerance, "tolerance", result.tolerance) ||
        !number(c.major_diameter, "major_diameter", false, result.major_diameter_mm) ||
        !number(c.minor_diameter, "minor_diameter", false, result.minor_diameter_mm) ||
        !number(c.pitch, "pitch", false, result.pitch_mm) ||
        !number(c.included_angle, "included_angle", true, result.included_angle_deg) ||
        (c.taper_angle && !number(*c.taper_angle, "taper_angle", true, result.taper_angle_deg)))
      return fail("Pinned Thread columns/cells require nonblank Text, Length or Angle values with "
                  "exact types");
    if (c.taper_angle && result.taper_angle_deg == 0)
      return fail("Explicit thread taper must be nonzero");
  }
  if (result.major_diameter_mm <= result.minor_diameter_mm || result.included_angle_deg <= 0 ||
      result.included_angle_deg >= 180 || std::abs(result.taper_angle_deg) >= 90)
    return fail("Thread requires major > minor diameter, an included angle in (0,180) and taper in "
                "(-90,90)");
  const double diameter_change =
      2 * std::tan(result.taper_angle_deg * std::numbers::pi / 180) * result.length_mm;
  if (!std::isfinite(diameter_change) ||
      !std::isfinite(result.major_diameter_mm + diameter_change) ||
      result.minor_diameter_mm + diameter_change <= 0 ||
      !std::isfinite(result.length_mm / result.pitch_mm))
    return fail("Thread interval overflows or taper reaches a nonpositive diameter");
  return Result<ResolvedThreadDefinition>::success(std::move(result));
}

ThreadFeature::ThreadFeature(FeatureId id, std::string name, BodyId target_body,
                             FaceReference support, ThreadDefinition thread,
                             ThreadDirection direction)
    : id_(std::move(id)), name_(std::move(name)), target_body_(std::move(target_body)),
      support_(std::move(support)), thread_(std::move(thread)), direction_(direction) {}
Result<ThreadFeature> ThreadFeature::create(FeatureId id, std::string name, BodyId target_body,
                                            FaceReference support, ThreadDefinition thread,
                                            ThreadDirection direction) {
  if (id.empty() || !nonblank(name) || target_body.empty() ||
      support.role() != PartFeatureInputRole::ThreadSupportFace ||
      (direction != ThreadDirection::AlongSupportAxis &&
       direction != ThreadDirection::OppositeSupportAxis))
    return Result<ThreadFeature>::failure(Error::validation(
        id.empty() ? "thread" : id.value(),
        "ThreadFeature requires identity, name, target Body and thread_support_face"));
  return Result<ThreadFeature>::success(ThreadFeature(std::move(id), std::move(name),
                                                      std::move(target_body), std::move(support),
                                                      std::move(thread), direction));
}
Result<std::size_t> validate_thread_feature_inputs(const PartDocument& part,
                                                   const ThreadFeature& feature) {
  const auto* body = part.find_body(feature.target_body());
  if (part.is_reference_part() || !body || body->kind() != BodyKind::Solid)
    return Result<std::size_t>::failure(
        Error::validation(feature.id().value(), "Thread target requires an editable solid Body"));
  auto support = validate_part_feature_input_reference(part, feature.support());
  if (!support)
    return support;
  auto thread = resolve_thread_definition(part, feature.thread(), feature.id().value());
  if (!thread)
    return Result<std::size_t>::failure(thread.error());
  return Result<std::size_t>::success(1);
}
} // namespace blcad
