#include "blcad/core/direct_edit_feature.hpp"
#include "blcad/core/part_document.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace blcad {
namespace {
bool finite(Point3 p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
bool finite(Vector3 v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
std::string identity(const FaceReference& face) {
  return std::visit(
      [](const auto& ref) -> std::string {
        using T = std::decay_t<decltype(ref)>;
        if constexpr (std::is_same_v<T, ResultFaceReference>)
          return ref.identity();
        else if constexpr (std::is_same_v<T, ImportedTopologyReference>)
          return make_imported_topology_target_spelling(ref).value();
        else if constexpr (std::is_same_v<T, SemanticFaceReference>)
          return ref.source_feature().value() + "/" + std::string(to_string(ref.face()));
        else
          return ref.source_feature().value() + "/" + ref.source_profile().value() + "/wall";
      },
      face.source());
}
bool protected_origin(const PartDocument& part, FeatureId id) {
  return part.find_hole_feature(id) || part.find_thread_feature(id);
}
} // namespace

DirectEditFeature::DirectEditFeature(FeatureId id, std::string name, BodyId target,
                                     FeatureId predecessor, std::vector<DirectEditFace> selections,
                                     DirectEditOperation operation, DirectEditFrame frame,
                                     DirectEditPolicy policy)
    : id_(std::move(id)), name_(std::move(name)), target_(std::move(target)),
      predecessor_(std::move(predecessor)), selections_(std::move(selections)),
      operation_(std::move(operation)), frame_(frame), policy_(policy) {}

Result<DirectEditFeature>
DirectEditFeature::create(FeatureId id, std::string name, BodyId target, FeatureId predecessor,
                          std::vector<DirectEditFace> selections, DirectEditOperation operation,
                          DirectEditFrame frame, DirectEditPolicy policy) {
  const auto fail = [&](const char* message) {
    return Result<DirectEditFeature>::failure(Error::validation(id.value(), message));
  };
  if (id.empty() || name.empty() || target.empty() || predecessor.empty() || predecessor == id ||
      selections.empty())
    return fail("Direct Edit requires identity, name, target, predecessor and selected faces");
  if (!finite(frame.origin_mm) || !finite(frame.x_axis) || !finite(frame.y_axis) ||
      std::abs(length(frame.x_axis) - 1) > 1e-10 || std::abs(length(frame.y_axis) - 1) > 1e-10 ||
      std::abs(dot(frame.x_axis, frame.y_axis)) > 1e-10)
    return fail("Direct Edit frame requires a finite origin and orthonormal right-handed axes");
  if (!std::isfinite(policy.tolerance_mm) || policy.tolerance_mm < 1e-7 ||
      policy.tolerance_mm > 1e-3 || policy.blends != DirectEditBlendPolicy::RejectAffectedBlends ||
      policy.healing != DirectEditHealPolicy::ExtendAdjacent)
    return fail("Direct Edit requires an explicit supported blend/heal policy and tolerance");
  std::set<std::string> unique;
  for (const auto& face : selections) {
    if (face.reference.role() != PartFeatureInputRole::SurfaceSourceFace ||
        (face.expected != DirectEditFaceCapability::Planar &&
         face.expected != DirectEditFaceCapability::Cylindrical &&
         face.expected != DirectEditFaceCapability::AnySurface) ||
        !unique.insert(identity(face.reference)).second)
      return fail("Direct Edit requires unique semantic surface faces with known capabilities");
    if ((std::holds_alternative<DirectFaceMove>(operation) ||
         std::holds_alternative<DirectFaceRotate>(operation)) &&
        face.expected != DirectEditFaceCapability::Planar)
      return fail("Direct face Move/Rotate requires explicitly planar supports");
    if (std::holds_alternative<DirectFaceSize>(operation) &&
        face.expected != DirectEditFaceCapability::Cylindrical)
      return fail("Direct face Size requires explicitly cylindrical supports");
    if (std::holds_alternative<DirectFaceOffset>(operation) &&
        face.expected == DirectEditFaceCapability::AnySurface)
      return fail("Direct face Offset requires explicitly planar or cylindrical supports");
  }
  const bool valid_operation = std::visit(
      [](const auto& op) {
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<T, DirectFaceMove>)
          return !op.x.empty() && !op.y.empty() && !op.z.empty() && finite(op.signs) &&
                 (op.signs.x == -1 || op.signs.x == 0 || op.signs.x == 1) &&
                 (op.signs.y == -1 || op.signs.y == 0 || op.signs.y == 1) &&
                 (op.signs.z == -1 || op.signs.z == 0 || op.signs.z == 1) && length(op.signs) > 0;
        else if constexpr (std::is_same_v<T, DirectFaceRotate>)
          return finite(op.axis) && length(op.axis) > 1e-12 && !op.angle.empty();
        else if constexpr (std::is_same_v<T, DirectFaceOffset>)
          return !op.distance.empty();
        else if constexpr (std::is_same_v<T, DirectFaceSize>)
          return !op.size.empty() &&
                 (op.mode == DirectEditSizeMode::Radius || op.mode == DirectEditSizeMode::Diameter);
        else
          return true;
      },
      operation);
  if (!valid_operation)
    return fail("Direct Edit operation requires typed nonempty bindings and a valid axis/mode");
  return Result<DirectEditFeature>::success(
      DirectEditFeature(std::move(id), std::move(name), std::move(target), std::move(predecessor),
                        std::move(selections), std::move(operation), frame, policy));
}

std::vector<FaceReference> DirectEditFeature::faces() const {
  std::vector<FaceReference> out;
  for (const auto& face : selections_)
    out.push_back(face.reference);
  return out;
}
std::vector<ParameterId> DirectEditFeature::parameter_dependencies() const {
  auto out = std::visit(
      [](const auto& op) -> std::vector<ParameterId> {
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<T, DirectFaceMove>)
          return {op.x, op.y, op.z};
        else if constexpr (std::is_same_v<T, DirectFaceRotate>)
          return {op.angle};
        else if constexpr (std::is_same_v<T, DirectFaceOffset>)
          return {op.distance};
        else if constexpr (std::is_same_v<T, DirectFaceSize>)
          return {op.size};
        else
          return {};
      },
      operation_);
  std::sort(out.begin(), out.end(),
            [](const auto& a, const auto& b) { return a.value() < b.value(); });
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}
std::vector<std::string> DirectEditFeature::input_nodes() const {
  std::vector<std::string> out{predecessor_.value()};
  for (const auto& face : selections_) {
    const auto node = face.reference.source_node_id();
    if (std::find(out.begin(), out.end(), node) == out.end())
      out.push_back(node);
  }
  return out;
}

Result<std::size_t> validate_direct_edit_parameters(const PartDocument& p,
                                                    const DirectEditFeature& f) {
  const auto fail = [&](const char* message) {
    return Result<std::size_t>::failure(Error::validation(f.id().value(), message));
  };
  const bool angle = std::holds_alternative<DirectFaceRotate>(f.operation());
  for (const auto& id : f.parameter_dependencies()) {
    const auto* value = p.find_parameter(id);
    if (!value || value->type() != (angle ? ParameterType::Angle : ParameterType::Length))
      return fail("Direct Edit parameter is missing or has the wrong dimension");
    const double number = angle ? value->value().degrees() : value->value().millimeters();
    if (!std::isfinite(number))
      return fail("Direct Edit parameter must be finite");
    if (std::holds_alternative<DirectFaceSize>(f.operation()) && number <= f.policy().tolerance_mm)
      return fail("Direct Edit radius/diameter must exceed healing tolerance");
    if (angle && (std::abs(number) <= 1e-10 || std::abs(number) >= 90))
      return fail("Direct Edit planar rotation requires 0 < |angle| < 90 degrees");
  }
  return Result<std::size_t>::success(f.parameter_dependencies().size());
}

Result<std::size_t> validate_direct_edit_inputs(const PartDocument& p, const DirectEditFeature& f) {
  const auto fail = [&](const char* message) {
    return Result<std::size_t>::failure(Error::validation(f.id().value(), message));
  };
  const auto* body = p.find_body(f.target_body());
  if (p.is_reference_part() || !body || body->kind() != BodyKind::Solid)
    return fail("Direct Edit requires an editable solid Body; convert a Reference Part explicitly");
  if (!p.dependency_graph().has_node(f.predecessor().value()))
    return fail("Direct Edit predecessor is unavailable");
  for (const auto& face : f.selections()) {
    auto valid = validate_part_feature_input_reference(p, face.reference);
    if (!valid)
      return valid;
    const bool owned = std::visit(
        [&](const auto& ref) {
          using T = std::decay_t<decltype(ref)>;
          if constexpr (std::is_same_v<T, ResultFaceReference>) {
            if (protected_origin(p, ref.producer()))
              return false;
            for (const auto& origin : ref.origins())
              if (protected_origin(p, origin.feature))
                return false;
            return ref.body() == f.target_body();
          } else if constexpr (std::is_same_v<T, ImportedTopologyReference>) {
            const auto* catalog = p.find_imported_topology_catalog(ref.catalog());
            const auto* entry = catalog ? catalog->find(ref.entry()) : nullptr;
            if (!entry || entry->descriptor.kind != ImportedTopologyKind::Face)
              return false;
            const auto geometry = entry->descriptor.geometry;
            if ((face.expected == DirectEditFaceCapability::Planar &&
                 geometry != ImportedGeometryClass::Plane) ||
                (face.expected == DirectEditFaceCapability::Cylindrical &&
                 geometry != ImportedGeometryClass::Cylinder))
              return false;
            for (const auto& base : p.imported_body_features())
              if (base.definition().catalog() == catalog->id())
                for (const auto& output : base.definition().outputs())
                  if (output.imported_body == entry->parent_body && output.body == f.target_body())
                    return true;
            return false;
          } else {
            if (protected_origin(p, ref.source_feature()))
              return false;
            const auto* source = p.find_feature(ref.source_feature());
            return source && source->body_result_context() &&
                   source->body_result_context()->effective_produced_body() == f.target_body() &&
                   (std::is_same_v<T, SemanticFaceReference>
                        ? face.expected != DirectEditFaceCapability::Cylindrical
                        : face.expected != DirectEditFaceCapability::Planar);
          }
        },
        face.reference.source());
    if (!owned)
      return fail("Direct Edit face has incompatible Body/capability or authored Hole/Thread; edit "
                  "its parameters");
  }
  return validate_direct_edit_parameters(p, f);
}
} // namespace blcad
