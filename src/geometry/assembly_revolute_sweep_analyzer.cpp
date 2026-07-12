#include "blcad/geometry/assembly_revolute_sweep_analyzer.hpp"

#include "blcad/geometry/assembly_cross_hierarchy_joint_motion_solver.hpp"
#include "blcad/geometry/assembly_joint_motion_solver.hpp"

#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace blcad::geometry {
namespace {

constexpr const char* kSweepAnalyzerId = "geometry.assembly_revolute_sweep_analyzer";

[[nodiscard]] Error validation_error(std::string message) {
  return Error::validation(kSweepAnalyzerId, std::move(message));
}

[[nodiscard]] std::string sample_text(std::size_t sample_index, double coordinate_deg) {
  std::ostringstream stream;
  stream << "sample " << sample_index << " at " << std::setprecision(17) << coordinate_deg
         << " deg";
  return stream.str();
}

[[nodiscard]] Error sample_error(std::size_t sample_index, double coordinate_deg,
                                 std::string stage, const Error& error) {
  return Error(error.category(), kSweepAnalyzerId,
               sample_text(sample_index, coordinate_deg) + " " + std::move(stage) +
                   " failed: [" + std::string(to_string(error.category())) + "] " +
                   error.object_id() + ": " + error.message());
}

[[nodiscard]] bool interval_within_limits(double start_deg, double end_deg,
                                          const AssemblyJointLimits& limits) noexcept {
  return start_deg >= limits.lower_deg && start_deg <= limits.upper_deg &&
         end_deg >= limits.lower_deg && end_deg <= limits.upper_deg;
}

[[nodiscard]] Result<std::size_t> validate_selected_joint(
    const Project& project, const AssemblyRevoluteSweepRequest& request) {
  if (request.joint.joint.empty()) {
    return Result<std::size_t>::failure(
        validation_error("Revolute sweep joint id must not be empty"));
  }
  if (request.start_coordinate.kind() != QuantityKind::AngleDeg ||
      request.end_coordinate.kind() != QuantityKind::AngleDeg) {
    return Result<std::size_t>::failure(
        validation_error("Revolute sweep start and end coordinates must use degrees"));
  }
  if (request.sample_count < 2U ||
      request.sample_count > kAssemblyRevoluteSweepMaximumSampleCount) {
    return Result<std::size_t>::failure(validation_error(
        "Revolute sweep sample count must be between 2 and " +
        std::to_string(kAssemblyRevoluteSweepMaximumSampleCount)));
  }

  const double start_deg = request.start_coordinate.degrees();
  const double end_deg = request.end_coordinate.degrees();
  switch (request.joint.scope) {
  case AssemblyRevoluteSweepJointScope::RootAssemblyLocal: {
    const AssemblyJoint* joint = project.assembly().find_joint(request.joint.joint);
    if (joint == nullptr) {
      return Result<std::size_t>::failure(
          validation_error("root-local Revolute sweep joint must exist in the root assembly"));
    }
    if (joint->state() != AssemblyJointState::Active ||
        joint->type() != AssemblyJointType::Revolute) {
      return Result<std::size_t>::failure(
          validation_error("root-local Revolute sweep requires an active Revolute joint"));
    }
    if (!interval_within_limits(start_deg, end_deg, joint->limits())) {
      return Result<std::size_t>::failure(
          validation_error("root-local Revolute sweep interval must lie within joint limits"));
    }
    return Result<std::size_t>::success(1U);
  }
  case AssemblyRevoluteSweepJointScope::ProjectCrossHierarchy: {
    const AssemblyHierarchyJoint* joint = project.find_cross_hierarchy_joint(request.joint.joint);
    if (joint == nullptr) {
      return Result<std::size_t>::failure(
          validation_error("cross-hierarchy Revolute sweep joint must exist in the Project"));
    }
    if (joint->state() != AssemblyJointState::Active ||
        joint->type() != AssemblyJointType::Revolute) {
      return Result<std::size_t>::failure(
          validation_error("cross-hierarchy Revolute sweep requires an active Revolute joint"));
    }
    if (!interval_within_limits(start_deg, end_deg, joint->limits())) {
      return Result<std::size_t>::failure(
          validation_error("cross-hierarchy Revolute sweep interval must lie within joint limits"));
    }
    return Result<std::size_t>::success(1U);
  }
  }

  return Result<std::size_t>::failure(
      validation_error("Revolute sweep joint scope is unsupported"));
}

[[nodiscard]] double sample_coordinate(double start_deg, double end_deg,
                                       std::size_t sample_index, std::size_t sample_count) {
  if (sample_index == 0U) {
    return start_deg;
  }
  if (sample_index + 1U == sample_count) {
    return end_deg;
  }
  const double fraction = static_cast<double>(sample_index) /
                          static_cast<double>(sample_count - 1U);
  return start_deg + ((end_deg - start_deg) * fraction);
}

[[nodiscard]] Result<std::size_t> apply_sample_motion(
    Project& sample_project, const AssemblyRevoluteSweepJointReference& joint_reference,
    double coordinate_deg, std::size_t sample_index,
    const AssemblyRigidBodySolverOptions& solver_options) {
  auto coordinate = Quantity::angle_deg(coordinate_deg, joint_reference.joint.value());
  if (coordinate.has_error()) {
    return Result<std::size_t>::failure(
        sample_error(sample_index, coordinate_deg, "coordinate construction", coordinate.error()));
  }

  switch (joint_reference.scope) {
  case AssemblyRevoluteSweepJointScope::RootAssemblyLocal: {
    const AssemblyJointMotionSolver solver;
    auto moved = solver.move(sample_project, joint_reference.joint, coordinate.value(), solver_options);
    if (moved.has_error()) {
      return Result<std::size_t>::failure(
          sample_error(sample_index, coordinate_deg, "root-local motion solve", moved.error()));
    }
    if (!moved.value().converged()) {
      return Result<std::size_t>::failure(Error::geometry(
          kSweepAnalyzerId,
          sample_text(sample_index, coordinate_deg) + " root-local motion did not converge"));
    }
    const AssemblyJointMotionResultApplier applier;
    auto applied = applier.apply(sample_project, moved.value());
    if (applied.has_error()) {
      return Result<std::size_t>::failure(
          sample_error(sample_index, coordinate_deg, "root-local motion application",
                       applied.error()));
    }
    return applied;
  }
  case AssemblyRevoluteSweepJointScope::ProjectCrossHierarchy: {
    const AssemblyCrossHierarchyJointMotionSolver solver;
    auto moved = solver.move(sample_project, joint_reference.joint, coordinate.value(), solver_options);
    if (moved.has_error()) {
      return Result<std::size_t>::failure(sample_error(
          sample_index, coordinate_deg, "cross-hierarchy motion solve", moved.error()));
    }
    if (!moved.value().converged()) {
      return Result<std::size_t>::failure(Error::geometry(
          kSweepAnalyzerId,
          sample_text(sample_index, coordinate_deg) +
              " cross-hierarchy motion did not converge"));
    }
    const AssemblyCrossHierarchyJointMotionResultApplier applier;
    auto applied = applier.apply(sample_project, moved.value());
    if (applied.has_error()) {
      return Result<std::size_t>::failure(
          sample_error(sample_index, coordinate_deg, "cross-hierarchy motion application",
                       applied.error()));
    }
    return applied;
  }
  }

  return Result<std::size_t>::failure(Error::validation(
      kSweepAnalyzerId,
      sample_text(sample_index, coordinate_deg) + " Revolute sweep joint scope is unsupported"));
}

} // namespace

Result<AssemblyRevoluteSweepAnalysis> AssemblyRevoluteSweepAnalyzer::analyze(
    const Project& project, const AssemblyRevoluteSweepRequest& request,
    AssemblyRevoluteSweepAnalysisOptions options) const {
  auto structure = project.validate_assembly_structure();
  if (structure.has_error()) {
    return Result<AssemblyRevoluteSweepAnalysis>::failure(structure.error());
  }
  auto valid_joint = validate_selected_joint(project, request);
  if (valid_joint.has_error()) {
    return Result<AssemblyRevoluteSweepAnalysis>::failure(valid_joint.error());
  }

  AssemblyRevoluteSweepAnalysis analysis{
      request.joint, request.start_coordinate.degrees(), request.end_coordinate.degrees(), {}};
  analysis.samples.reserve(request.sample_count);

  const AssemblyContactAnalyzer contact_analyzer;
  for (std::size_t sample_index = 0U; sample_index < request.sample_count; ++sample_index) {
    const double coordinate_deg = sample_coordinate(
        analysis.start_coordinate_deg, analysis.end_coordinate_deg, sample_index,
        request.sample_count);
    Project sample_project = project;
    auto applied = apply_sample_motion(sample_project, request.joint, coordinate_deg, sample_index,
                                       options.motion_solver);
    if (applied.has_error()) {
      return Result<AssemblyRevoluteSweepAnalysis>::failure(applied.error());
    }

    auto contact = contact_analyzer.analyze(sample_project, options.contact);
    if (contact.has_error()) {
      return Result<AssemblyRevoluteSweepAnalysis>::failure(
          sample_error(sample_index, coordinate_deg, "contact analysis", contact.error()));
    }
    analysis.samples.push_back(AssemblyRevoluteSweepSample{
        sample_index, coordinate_deg, applied.value(), std::move(contact.value())});
  }

  return Result<AssemblyRevoluteSweepAnalysis>::success(std::move(analysis));
}

} // namespace blcad::geometry
