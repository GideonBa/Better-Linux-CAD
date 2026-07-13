#include "blcad/core/assembly_joint.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace blcad {
namespace {

[[nodiscard]] bool is_valid(AssemblyJointCoordinateKind kind) noexcept {
  switch (kind) {
  case AssemblyJointCoordinateKind::Angular:
  case AssemblyJointCoordinateKind::Linear:
    return true;
  }
  return false;
}

[[nodiscard]] bool is_valid(AssemblyJointCoordinateRole role) noexcept {
  switch (role) {
  case AssemblyJointCoordinateRole::Rotation:
  case AssemblyJointCoordinateRole::Translation:
  case AssemblyJointCoordinateRole::TranslationU:
  case AssemblyJointCoordinateRole::TranslationV:
  case AssemblyJointCoordinateRole::RotationNormal:
    return true;
  }
  return false;
}

[[nodiscard]] QuantityKind quantity_kind(AssemblyJointCoordinateKind kind) noexcept {
  switch (kind) {
  case AssemblyJointCoordinateKind::Angular:
    return QuantityKind::AngleDeg;
  case AssemblyJointCoordinateKind::Linear:
    return QuantityKind::LinearDisplacementMm;
  }
  return QuantityKind::AngleDeg;
}

[[nodiscard]] AssemblyJointCoordinateKind role_kind(AssemblyJointCoordinateRole role) noexcept {
  switch (role) {
  case AssemblyJointCoordinateRole::Rotation:
  case AssemblyJointCoordinateRole::RotationNormal:
    return AssemblyJointCoordinateKind::Angular;
  case AssemblyJointCoordinateRole::Translation:
  case AssemblyJointCoordinateRole::TranslationU:
  case AssemblyJointCoordinateRole::TranslationV:
    return AssemblyJointCoordinateKind::Linear;
  }
  return AssemblyJointCoordinateKind::Angular;
}

[[nodiscard]] double quantity_value(const Quantity& quantity,
                                    AssemblyJointCoordinateKind kind) noexcept {
  return kind == AssemblyJointCoordinateKind::Angular ? quantity.degrees() : quantity.millimeters();
}

[[nodiscard]] bool quantity_equal(const Quantity& lhs, const Quantity& rhs) noexcept {
  if (lhs.kind() != rhs.kind())
    return false;
  switch (lhs.kind()) {
  case QuantityKind::AngleDeg:
    return lhs.degrees() == rhs.degrees();
  case QuantityKind::LengthMm:
  case QuantityKind::LinearDisplacementMm:
    return lhs.millimeters() == rhs.millimeters();
  case QuantityKind::Count:
    return lhs.count_value() == rhs.count_value();
  }
  return false;
}

[[nodiscard]] bool optional_quantity_equal(const std::optional<Quantity>& lhs,
                                           const std::optional<Quantity>& rhs) noexcept {
  if (lhs.has_value() != rhs.has_value())
    return false;
  return !lhs || quantity_equal(*lhs, *rhs);
}

[[nodiscard]] Result<AssemblyJointLimits>
validate_family_coordinates(const std::string& object_id, AssemblyJointType type,
                            const std::vector<AssemblyJointCoordinateSlot>& slots) {
  switch (type) {
  case AssemblyJointType::Revolute: {
    if (slots.size() != 1U || slots.front().role() != AssemblyJointCoordinateRole::Rotation ||
        slots.front().kind() != AssemblyJointCoordinateKind::Angular) {
      return Result<AssemblyJointLimits>::failure(Error::validation(
          object_id, "revolute joint requires exactly one angular rotation coordinate slot"));
    }
    if (!slots.front().lower_limit() || !slots.front().upper_limit()) {
      return Result<AssemblyJointLimits>::failure(Error::validation(
          object_id, "revolute joint rotation coordinate requires lower and upper limits"));
    }
    const double lower_deg = slots.front().lower_limit()->degrees();
    const double upper_deg = slots.front().upper_limit()->degrees();
    if (lower_deg < -180.0 || upper_deg > 180.0) {
      return Result<AssemblyJointLimits>::failure(Error::validation(
          object_id,
          "revolute joint seed limits must stay within the principal -180 to 180 degree range"));
    }
    return Result<AssemblyJointLimits>::success({lower_deg, upper_deg});
  }
  case AssemblyJointType::Prismatic:
    if (slots.size() != 1U || slots.front().role() != AssemblyJointCoordinateRole::Translation ||
        slots.front().kind() != AssemblyJointCoordinateKind::Linear) {
      return Result<AssemblyJointLimits>::failure(Error::validation(
          object_id, "prismatic joint requires exactly one linear translation coordinate slot"));
    }
    if (!slots.front().lower_limit() || !slots.front().upper_limit()) {
      return Result<AssemblyJointLimits>::failure(Error::validation(
          object_id, "prismatic joint translation coordinate requires lower and upper limits"));
    }
    return Result<AssemblyJointLimits>::success({});
  case AssemblyJointType::Cylindrical: {
    if (slots.size() != 2U || slots[0].role() != AssemblyJointCoordinateRole::Translation ||
        slots[0].kind() != AssemblyJointCoordinateKind::Linear ||
        slots[1].role() != AssemblyJointCoordinateRole::Rotation ||
        slots[1].kind() != AssemblyJointCoordinateKind::Angular) {
      return Result<AssemblyJointLimits>::failure(Error::validation(
          object_id,
          "cylindrical joint requires ordered linear translation and angular rotation slots"));
    }
    if (!slots[0].lower_limit() || !slots[0].upper_limit() || !slots[1].lower_limit() ||
        !slots[1].upper_limit()) {
      return Result<AssemblyJointLimits>::failure(Error::validation(
          object_id, "cylindrical joint coordinates require lower and upper limits"));
    }
    if (slots[1].lower_limit()->degrees() < -180.0 || slots[1].upper_limit()->degrees() > 180.0) {
      return Result<AssemblyJointLimits>::failure(Error::validation(
          object_id,
          "cylindrical rotation limits must stay within the principal -180 to 180 degree range"));
    }
    return Result<AssemblyJointLimits>::success({});
  }
  }
  return Result<AssemblyJointLimits>::failure(
      Error::validation(object_id, "unsupported assembly joint family"));
}

[[nodiscard]] Result<AssemblyJointCoordinateSlot> make_revolute_slot(const std::string& object_id,
                                                                     Quantity lower_limit,
                                                                     Quantity upper_limit,
                                                                     Quantity coordinate) {
  return AssemblyJointCoordinateSlot::create(
      AssemblyJointCoordinateRole::Rotation, AssemblyJointCoordinateKind::Angular,
      std::move(coordinate), std::move(lower_limit), std::move(upper_limit), object_id);
}

} // namespace

std::string_view to_string(AssemblyJointType type) noexcept {
  switch (type) {
  case AssemblyJointType::Revolute:
    return "revolute";
  case AssemblyJointType::Prismatic:
    return "prismatic";
  case AssemblyJointType::Cylindrical:
    return "cylindrical";
  }
  return "revolute";
}

std::string_view to_string(AssemblyJointState state) noexcept {
  switch (state) {
  case AssemblyJointState::Active:
    return "active";
  case AssemblyJointState::Inactive:
    return "inactive";
  }
  return "active";
}

std::string_view to_string(AssemblyJointCoordinateKind kind) noexcept {
  switch (kind) {
  case AssemblyJointCoordinateKind::Angular:
    return "angular";
  case AssemblyJointCoordinateKind::Linear:
    return "linear";
  }
  return "angular";
}

std::string_view to_string(AssemblyJointCoordinateRole role) noexcept {
  switch (role) {
  case AssemblyJointCoordinateRole::Rotation:
    return "rotation";
  case AssemblyJointCoordinateRole::Translation:
    return "translation";
  case AssemblyJointCoordinateRole::TranslationU:
    return "translation_u";
  case AssemblyJointCoordinateRole::TranslationV:
    return "translation_v";
  case AssemblyJointCoordinateRole::RotationNormal:
    return "rotation_normal";
  }
  return "rotation";
}

Result<AssemblyJointCoordinateSlot> AssemblyJointCoordinateSlot::create(
    AssemblyJointCoordinateRole role, AssemblyJointCoordinateKind kind, Quantity value,
    std::optional<Quantity> lower_limit, std::optional<Quantity> upper_limit,
    std::string_view object_id) {
  if (!is_valid(role) || !is_valid(kind)) {
    return Result<AssemblyJointCoordinateSlot>::failure(Error::validation(
        std::string(object_id), "assembly joint coordinate role and kind must be known"));
  }
  if (kind != role_kind(role)) {
    return Result<AssemblyJointCoordinateSlot>::failure(Error::validation(
        std::string(object_id), "assembly joint coordinate role has an incompatible kind"));
  }
  const QuantityKind expected_kind = quantity_kind(kind);
  if (value.kind() != expected_kind || (lower_limit && lower_limit->kind() != expected_kind) ||
      (upper_limit && upper_limit->kind() != expected_kind)) {
    return Result<AssemblyJointCoordinateSlot>::failure(Error::validation(
        std::string(object_id), "assembly joint coordinate value and limits must use its kind"));
  }

  const double coordinate_value = quantity_value(value, kind);
  if (lower_limit && upper_limit &&
      quantity_value(*lower_limit, kind) >= quantity_value(*upper_limit, kind)) {
    return Result<AssemblyJointCoordinateSlot>::failure(
        Error::validation(std::string(object_id),
                          "assembly joint coordinate lower limit must be less than upper limit"));
  }
  if (lower_limit && coordinate_value < quantity_value(*lower_limit, kind)) {
    return Result<AssemblyJointCoordinateSlot>::failure(Error::validation(
        std::string(object_id), "assembly joint coordinate must not be below its lower limit"));
  }
  if (upper_limit && coordinate_value > quantity_value(*upper_limit, kind)) {
    return Result<AssemblyJointCoordinateSlot>::failure(Error::validation(
        std::string(object_id), "assembly joint coordinate must not exceed its upper limit"));
  }

  return Result<AssemblyJointCoordinateSlot>::success(AssemblyJointCoordinateSlot(
      role, kind, std::move(value), std::move(lower_limit), std::move(upper_limit)));
}

AssemblyJointCoordinateSlot::AssemblyJointCoordinateSlot(AssemblyJointCoordinateRole role,
                                                         AssemblyJointCoordinateKind kind,
                                                         Quantity value,
                                                         std::optional<Quantity> lower_limit,
                                                         std::optional<Quantity> upper_limit)
    : role_(role), kind_(kind), value_(std::move(value)), lower_limit_(std::move(lower_limit)),
      upper_limit_(std::move(upper_limit)) {}

AssemblyJointCoordinateRole AssemblyJointCoordinateSlot::role() const noexcept {
  return role_;
}
AssemblyJointCoordinateKind AssemblyJointCoordinateSlot::kind() const noexcept {
  return kind_;
}
const Quantity& AssemblyJointCoordinateSlot::value() const noexcept {
  return value_;
}
const std::optional<Quantity>& AssemblyJointCoordinateSlot::lower_limit() const noexcept {
  return lower_limit_;
}
const std::optional<Quantity>& AssemblyJointCoordinateSlot::upper_limit() const noexcept {
  return upper_limit_;
}

bool operator==(const AssemblyJointCoordinateSlot& lhs,
                const AssemblyJointCoordinateSlot& rhs) noexcept {
  return lhs.role_ == rhs.role_ && lhs.kind_ == rhs.kind_ &&
         quantity_equal(lhs.value_, rhs.value_) &&
         optional_quantity_equal(lhs.lower_limit_, rhs.lower_limit_) &&
         optional_quantity_equal(lhs.upper_limit_, rhs.upper_limit_);
}

Result<AssemblyJoint> AssemblyJoint::create(AssemblyJointId id, std::string name,
                                            AssemblyJointType type,
                                            AssemblyConstraintTarget target_a,
                                            AssemblyConstraintTarget target_b,
                                            AssemblyJointState state, Quantity lower_limit,
                                            Quantity upper_limit, Quantity coordinate) {
  const auto object_id = id.empty() ? std::string("assembly_joint") : id.value();
  auto slot = make_revolute_slot(object_id, std::move(lower_limit), std::move(upper_limit),
                                 std::move(coordinate));
  if (slot.has_error())
    return Result<AssemblyJoint>::failure(slot.error());
  std::vector<AssemblyJointCoordinateSlot> slots;
  slots.push_back(slot.value());
  return create(std::move(id), std::move(name), type, std::move(target_a), std::move(target_b),
                state, std::move(slots));
}

Result<AssemblyJoint>
AssemblyJoint::create(AssemblyJointId id, std::string name, AssemblyJointType type,
                      AssemblyConstraintTarget target_a, AssemblyConstraintTarget target_b,
                      AssemblyJointState state,
                      std::vector<AssemblyJointCoordinateSlot> coordinate_slots) {
  const auto object_id = id.empty() ? std::string("assembly_joint") : id.value();
  if (id.empty()) {
    return Result<AssemblyJoint>::failure(
        Error::validation(object_id, "assembly joint id must not be empty"));
  }
  if (name.empty()) {
    return Result<AssemblyJoint>::failure(
        Error::validation(object_id, "assembly joint name must not be empty"));
  }
  if (target_a.component_instance() == target_b.component_instance()) {
    return Result<AssemblyJoint>::failure(Error::validation(
        object_id, "assembly joint targets must reference distinct component instances"));
  }
  auto legacy_limits = validate_family_coordinates(object_id, type, coordinate_slots);
  if (legacy_limits.has_error())
    return Result<AssemblyJoint>::failure(legacy_limits.error());

  return Result<AssemblyJoint>::success(
      AssemblyJoint(std::move(id), std::move(name), type, std::move(target_a), std::move(target_b),
                    state, std::move(coordinate_slots), legacy_limits.value()));
}

Result<AssemblyJoint> AssemblyJoint::with_coordinate(Quantity coordinate) const {
  if (type_ != AssemblyJointType::Revolute) {
    return Result<AssemblyJoint>::failure(Error::validation(
        id_.value(), "scalar assembly joint coordinate updates support only Revolute joints"));
  }
  return with_coordinate_value(AssemblyJointCoordinateRole::Rotation, std::move(coordinate));
}

Result<AssemblyJoint> AssemblyJoint::with_coordinate_value(AssemblyJointCoordinateRole role,
                                                           Quantity coordinate) const {
  auto slots = coordinate_slots_;
  const auto found = std::find_if(slots.begin(), slots.end(),
                                  [role](const auto& slot) { return slot.role() == role; });
  if (found == slots.end()) {
    return Result<AssemblyJoint>::failure(Error::validation(
        id_.value(), "assembly joint coordinate role is not defined by its family"));
  }
  auto replacement =
      AssemblyJointCoordinateSlot::create(found->role(), found->kind(), std::move(coordinate),
                                          found->lower_limit(), found->upper_limit(), id_.value());
  if (replacement.has_error())
    return Result<AssemblyJoint>::failure(replacement.error());
  *found = replacement.value();
  return create(id_, name_, type_, target_a_, target_b_, state_, std::move(slots));
}

AssemblyJoint::AssemblyJoint(AssemblyJointId id, std::string name, AssemblyJointType type,
                             AssemblyConstraintTarget target_a, AssemblyConstraintTarget target_b,
                             AssemblyJointState state,
                             std::vector<AssemblyJointCoordinateSlot> coordinate_slots,
                             AssemblyJointLimits legacy_limits)
    : id_(std::move(id)), name_(std::move(name)), type_(type), target_a_(std::move(target_a)),
      target_b_(std::move(target_b)), state_(state), coordinate_slots_(std::move(coordinate_slots)),
      legacy_limits_(legacy_limits) {}

const AssemblyJointId& AssemblyJoint::id() const noexcept {
  return id_;
}
const std::string& AssemblyJoint::name() const noexcept {
  return name_;
}
AssemblyJointType AssemblyJoint::type() const noexcept {
  return type_;
}
const AssemblyConstraintTarget& AssemblyJoint::target_a() const noexcept {
  return target_a_;
}
const AssemblyConstraintTarget& AssemblyJoint::target_b() const noexcept {
  return target_b_;
}
AssemblyJointState AssemblyJoint::state() const noexcept {
  return state_;
}
const std::vector<AssemblyJointCoordinateSlot>& AssemblyJoint::coordinate_slots() const noexcept {
  return coordinate_slots_;
}
const AssemblyJointCoordinateSlot*
AssemblyJoint::find_coordinate_slot(AssemblyJointCoordinateRole role) const noexcept {
  const auto found = std::find_if(coordinate_slots_.begin(), coordinate_slots_.end(),
                                  [role](const auto& slot) { return slot.role() == role; });
  return found == coordinate_slots_.end() ? nullptr : &*found;
}
const AssemblyJointLimits& AssemblyJoint::limits() const noexcept {
  return legacy_limits_;
}
double AssemblyJoint::coordinate_deg() const noexcept {
  return type_ == AssemblyJointType::Revolute ? coordinate_slots_.front().value().degrees() : 0.0;
}

Result<AssemblyHierarchyJoint> AssemblyHierarchyJoint::create(
    AssemblyJointId id, std::string name, AssemblyJointType type,
    AssemblyHierarchyConstraintEndpoint target_a, AssemblyHierarchyConstraintEndpoint target_b,
    AssemblyJointState state, Quantity lower_limit, Quantity upper_limit, Quantity coordinate) {
  const auto object_id = id.empty() ? std::string("assembly_hierarchy_joint") : id.value();
  auto slot = make_revolute_slot(object_id, std::move(lower_limit), std::move(upper_limit),
                                 std::move(coordinate));
  if (slot.has_error())
    return Result<AssemblyHierarchyJoint>::failure(slot.error());
  std::vector<AssemblyJointCoordinateSlot> slots;
  slots.push_back(slot.value());
  return create(std::move(id), std::move(name), type, std::move(target_a), std::move(target_b),
                state, std::move(slots));
}

Result<AssemblyHierarchyJoint> AssemblyHierarchyJoint::create(
    AssemblyJointId id, std::string name, AssemblyJointType type,
    AssemblyHierarchyConstraintEndpoint target_a, AssemblyHierarchyConstraintEndpoint target_b,
    AssemblyJointState state, std::vector<AssemblyJointCoordinateSlot> coordinate_slots) {
  const auto object_id = id.empty() ? std::string("assembly_hierarchy_joint") : id.value();
  if (id.empty()) {
    return Result<AssemblyHierarchyJoint>::failure(
        Error::validation(object_id, "cross-hierarchy assembly joint id must not be empty"));
  }
  if (name.empty()) {
    return Result<AssemblyHierarchyJoint>::failure(
        Error::validation(object_id, "cross-hierarchy assembly joint name must not be empty"));
  }
  if (target_a == target_b) {
    return Result<AssemblyHierarchyJoint>::failure(Error::validation(
        object_id, "cross-hierarchy assembly joint targets must be distinct endpoint identities"));
  }
  auto legacy_limits = validate_family_coordinates(object_id, type, coordinate_slots);
  if (legacy_limits.has_error()) {
    return Result<AssemblyHierarchyJoint>::failure(legacy_limits.error());
  }

  return Result<AssemblyHierarchyJoint>::success(AssemblyHierarchyJoint(
      std::move(id), std::move(name), type, std::move(target_a), std::move(target_b), state,
      std::move(coordinate_slots), legacy_limits.value()));
}

Result<AssemblyHierarchyJoint> AssemblyHierarchyJoint::with_coordinate(Quantity coordinate) const {
  if (type_ != AssemblyJointType::Revolute) {
    return Result<AssemblyHierarchyJoint>::failure(Error::validation(
        id_.value(),
        "scalar cross-hierarchy joint coordinate updates support only Revolute joints"));
  }
  return with_coordinate_value(AssemblyJointCoordinateRole::Rotation, std::move(coordinate));
}

Result<AssemblyHierarchyJoint>
AssemblyHierarchyJoint::with_coordinate_value(AssemblyJointCoordinateRole role,
                                              Quantity coordinate) const {
  auto slots = coordinate_slots_;
  const auto found = std::find_if(slots.begin(), slots.end(),
                                  [role](const auto& slot) { return slot.role() == role; });
  if (found == slots.end()) {
    return Result<AssemblyHierarchyJoint>::failure(Error::validation(
        id_.value(),
        "cross-hierarchy assembly joint coordinate role is not defined by its family"));
  }
  auto replacement =
      AssemblyJointCoordinateSlot::create(found->role(), found->kind(), std::move(coordinate),
                                          found->lower_limit(), found->upper_limit(), id_.value());
  if (replacement.has_error()) {
    return Result<AssemblyHierarchyJoint>::failure(replacement.error());
  }
  *found = replacement.value();
  return create(id_, name_, type_, target_a_, target_b_, state_, std::move(slots));
}

AssemblyHierarchyJoint::AssemblyHierarchyJoint(
    AssemblyJointId id, std::string name, AssemblyJointType type,
    AssemblyHierarchyConstraintEndpoint target_a, AssemblyHierarchyConstraintEndpoint target_b,
    AssemblyJointState state, std::vector<AssemblyJointCoordinateSlot> coordinate_slots,
    AssemblyJointLimits legacy_limits)
    : id_(std::move(id)), name_(std::move(name)), type_(type), target_a_(std::move(target_a)),
      target_b_(std::move(target_b)), state_(state), coordinate_slots_(std::move(coordinate_slots)),
      legacy_limits_(legacy_limits) {}

const AssemblyJointId& AssemblyHierarchyJoint::id() const noexcept {
  return id_;
}
const std::string& AssemblyHierarchyJoint::name() const noexcept {
  return name_;
}
AssemblyJointType AssemblyHierarchyJoint::type() const noexcept {
  return type_;
}
const AssemblyHierarchyConstraintEndpoint& AssemblyHierarchyJoint::target_a() const noexcept {
  return target_a_;
}
const AssemblyHierarchyConstraintEndpoint& AssemblyHierarchyJoint::target_b() const noexcept {
  return target_b_;
}
AssemblyJointState AssemblyHierarchyJoint::state() const noexcept {
  return state_;
}
const std::vector<AssemblyJointCoordinateSlot>&
AssemblyHierarchyJoint::coordinate_slots() const noexcept {
  return coordinate_slots_;
}
const AssemblyJointCoordinateSlot*
AssemblyHierarchyJoint::find_coordinate_slot(AssemblyJointCoordinateRole role) const noexcept {
  const auto found = std::find_if(coordinate_slots_.begin(), coordinate_slots_.end(),
                                  [role](const auto& slot) { return slot.role() == role; });
  return found == coordinate_slots_.end() ? nullptr : &*found;
}
const AssemblyJointLimits& AssemblyHierarchyJoint::limits() const noexcept {
  return legacy_limits_;
}
double AssemblyHierarchyJoint::coordinate_deg() const noexcept {
  return type_ == AssemblyJointType::Revolute ? coordinate_slots_.front().value().degrees() : 0.0;
}

} // namespace blcad
