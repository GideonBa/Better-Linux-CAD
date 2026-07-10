#include "blcad/core/sketch_diagnostics.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace blcad {
namespace {

using json = nlohmann::json;

[[nodiscard]] std::string endpoint_key(const SketchEntityId& id, SketchReferenceTargetKind kind) {
  const char* suffix = kind == SketchReferenceTargetKind::LineSegmentStart ? ":start" : ":end";
  return id.value() + suffix;
}

[[nodiscard]] bool is_endpoint_target(const SketchReferenceTarget& target) noexcept {
  return target.kind() == SketchReferenceTargetKind::LineSegmentStart ||
         target.kind() == SketchReferenceTargetKind::LineSegmentEnd;
}

[[nodiscard]] std::string target_key(const SketchReferenceTarget& target) {
  return std::string(to_string(target.kind())) + ":" + target.entity().value();
}

[[nodiscard]] std::string dimension_target_key(const SketchDrivingDimension& dimension) {
  std::string first = target_key(dimension.first_target());
  std::string second = target_key(dimension.second_target());
  if (second < first) std::swap(first, second);
  return first + "|" + second;
}

[[nodiscard]] bool has_any_profile(const Sketch& sketch) noexcept {
  return !sketch.rectangle_profiles().empty() || !sketch.circle_profiles().empty() ||
         !sketch.closed_profiles().empty() || !sketch.arc_closed_profiles().empty() ||
         !sketch.composite_closed_profiles().empty();
}

void add_endpoint_if_present(std::set<std::string>& constrained_endpoints,
                             const SketchReferenceTarget& target) {
  if (is_endpoint_target(target)) {
    constrained_endpoints.insert(endpoint_key(target.entity(), target.kind()));
  }
}

void add_line_endpoints(std::set<std::string>& constrained_endpoints, const SketchEntityId& line_id) {
  constrained_endpoints.insert(endpoint_key(line_id, SketchReferenceTargetKind::LineSegmentStart));
  constrained_endpoints.insert(endpoint_key(line_id, SketchReferenceTargetKind::LineSegmentEnd));
}

void add_diagnostic(SketchDiagnosticReport& report, SketchDiagnosticSeverity severity,
                    SketchDiagnosticKind kind, std::string target, std::string message) {
  report.add(SketchConstraintDiagnostic(severity, kind, std::move(target), std::move(message)));
}

} // namespace

std::string_view to_string(SketchDiagnosticSeverity severity) noexcept {
  switch (severity) {
  case SketchDiagnosticSeverity::Info: return "info";
  case SketchDiagnosticSeverity::Warning: return "warning";
  case SketchDiagnosticSeverity::Error: return "error";
  }
  return "info";
}

std::string_view to_string(SketchDiagnosticKind kind) noexcept {
  switch (kind) {
  case SketchDiagnosticKind::UnconstrainedLineEndpoint: return "unconstrained_line_endpoint";
  case SketchDiagnosticKind::FreeSplineControlPoint: return "free_spline_control_point";
  case SketchDiagnosticKind::ProfileWithoutDrivingDimensions:
    return "profile_without_driving_dimensions";
  case SketchDiagnosticKind::ConflictingHorizontalVerticalConstraints:
    return "conflicting_horizontal_vertical_constraints";
  case SketchDiagnosticKind::RedundantOrientationConstraint:
    return "redundant_orientation_constraint";
  case SketchDiagnosticKind::DuplicateFixedEndpoint: return "duplicate_fixed_endpoint";
  case SketchDiagnosticKind::DuplicateDrivingDimensionTarget:
    return "duplicate_driving_dimension_target";
  }
  return "unconstrained_line_endpoint";
}

SketchConstraintDiagnostic::SketchConstraintDiagnostic(SketchDiagnosticSeverity severity,
                                                       SketchDiagnosticKind kind,
                                                       std::string target,
                                                       std::string message)
    : severity_(severity), kind_(kind), target_(std::move(target)), message_(std::move(message)) {}

SketchDiagnosticSeverity SketchConstraintDiagnostic::severity() const noexcept { return severity_; }
SketchDiagnosticKind SketchConstraintDiagnostic::kind() const noexcept { return kind_; }
const std::string& SketchConstraintDiagnostic::target() const noexcept { return target_; }
const std::string& SketchConstraintDiagnostic::message() const noexcept { return message_; }

SketchDiagnosticReport::SketchDiagnosticReport(SketchId sketch_id) : sketch_id_(std::move(sketch_id)) {}

const SketchId& SketchDiagnosticReport::sketch_id() const noexcept { return sketch_id_; }
const std::vector<SketchConstraintDiagnostic>& SketchDiagnosticReport::diagnostics() const noexcept {
  return diagnostics_;
}

std::size_t SketchDiagnosticReport::warning_count() const noexcept {
  return static_cast<std::size_t>(std::count_if(
      diagnostics_.begin(), diagnostics_.end(), [](const SketchConstraintDiagnostic& diagnostic) {
        return diagnostic.severity() == SketchDiagnosticSeverity::Warning;
      }));
}

std::size_t SketchDiagnosticReport::error_count() const noexcept {
  return static_cast<std::size_t>(std::count_if(
      diagnostics_.begin(), diagnostics_.end(), [](const SketchConstraintDiagnostic& diagnostic) {
        return diagnostic.severity() == SketchDiagnosticSeverity::Error;
      }));
}

bool SketchDiagnosticReport::has_errors() const noexcept { return error_count() > 0U; }

void SketchDiagnosticReport::add(SketchConstraintDiagnostic diagnostic) {
  diagnostics_.push_back(std::move(diagnostic));
}

SketchDiagnosticReport SketchConstraintDiagnostics::analyze(const Sketch& sketch) const {
  SketchDiagnosticReport report(sketch.id());

  std::set<std::string> constrained_endpoints;
  std::map<std::string, std::size_t> fixed_endpoint_counts;
  std::map<std::string, std::size_t> horizontal_counts;
  std::map<std::string, std::size_t> vertical_counts;

  for (const auto& constraint : sketch.constraints()) {
    if (constraint.kind() == SketchConstraintKind::CoincidentToProjectedPoint) {
      add_endpoint_if_present(constrained_endpoints, constraint.constrained_target());
    }
  }

  for (const auto& constraint : sketch.geometric_constraints()) {
    if (constraint.kind() == SketchGeometricConstraintKind::Fixed) {
      if (constraint.first_target().kind() == SketchReferenceTargetKind::LineSegment) {
        add_line_endpoints(constrained_endpoints, constraint.first_target().entity());
      } else if (is_endpoint_target(constraint.first_target())) {
        const std::string key = endpoint_key(constraint.first_target().entity(),
                                             constraint.first_target().kind());
        constrained_endpoints.insert(key);
        ++fixed_endpoint_counts[key];
      }
    }

    if (constraint.kind() == SketchGeometricConstraintKind::Horizontal &&
        constraint.first_target().kind() == SketchReferenceTargetKind::LineSegment) {
      ++horizontal_counts[constraint.first_target().entity().value()];
    }
    if (constraint.kind() == SketchGeometricConstraintKind::Vertical &&
        constraint.first_target().kind() == SketchReferenceTargetKind::LineSegment) {
      ++vertical_counts[constraint.first_target().entity().value()];
    }
  }

  std::map<std::string, std::size_t> dimension_target_counts;
  for (const auto& dimension : sketch.driving_dimensions()) {
    add_endpoint_if_present(constrained_endpoints, dimension.first_target());
    add_endpoint_if_present(constrained_endpoints, dimension.second_target());
    ++dimension_target_counts[dimension_target_key(dimension)];
  }

  for (const auto& line : sketch.line_segments()) {
    const std::string start_key = endpoint_key(line.id(), SketchReferenceTargetKind::LineSegmentStart);
    if (!constrained_endpoints.contains(start_key)) {
      add_diagnostic(report, SketchDiagnosticSeverity::Warning,
                     SketchDiagnosticKind::UnconstrainedLineEndpoint, start_key,
                     "line segment start endpoint is not constrained by the current deterministic subset");
    }

    const std::string end_key = endpoint_key(line.id(), SketchReferenceTargetKind::LineSegmentEnd);
    if (!constrained_endpoints.contains(end_key)) {
      add_diagnostic(report, SketchDiagnosticSeverity::Warning,
                     SketchDiagnosticKind::UnconstrainedLineEndpoint, end_key,
                     "line segment end endpoint is not constrained by the current deterministic subset");
    }
  }

  for (const auto& spline : sketch.spline_segments()) {
    add_diagnostic(report, SketchDiagnosticSeverity::Warning,
                   SketchDiagnosticKind::FreeSplineControlPoint,
                   spline.id().value() + ":control1",
                   "spline control point is free in the current non-solver diagnostic subset");
    add_diagnostic(report, SketchDiagnosticSeverity::Warning,
                   SketchDiagnosticKind::FreeSplineControlPoint,
                   spline.id().value() + ":control2",
                   "spline control point is free in the current non-solver diagnostic subset");
  }

  if (has_any_profile(sketch) && sketch.driving_dimensions().empty()) {
    add_diagnostic(report, SketchDiagnosticSeverity::Warning,
                   SketchDiagnosticKind::ProfileWithoutDrivingDimensions,
                   sketch.id().value(),
                   "sketch has profile intent but no driving dimensions");
  }

  for (const auto& [line_id, horizontal_count] : horizontal_counts) {
    if (vertical_counts.contains(line_id)) {
      add_diagnostic(report, SketchDiagnosticSeverity::Error,
                     SketchDiagnosticKind::ConflictingHorizontalVerticalConstraints, line_id,
                     "line has both horizontal and vertical constraints in the deterministic subset");
    }
    if (horizontal_count > 1U) {
      add_diagnostic(report, SketchDiagnosticSeverity::Warning,
                     SketchDiagnosticKind::RedundantOrientationConstraint, line_id,
                     "line has duplicate horizontal constraints");
    }
  }

  for (const auto& [line_id, vertical_count] : vertical_counts) {
    if (vertical_count > 1U) {
      add_diagnostic(report, SketchDiagnosticSeverity::Warning,
                     SketchDiagnosticKind::RedundantOrientationConstraint, line_id,
                     "line has duplicate vertical constraints");
    }
  }

  for (const auto& [endpoint, count] : fixed_endpoint_counts) {
    if (count > 1U) {
      add_diagnostic(report, SketchDiagnosticSeverity::Error,
                     SketchDiagnosticKind::DuplicateFixedEndpoint, endpoint,
                     "endpoint has multiple fixed constraints; this is treated as an impossible fixed endpoint pair in the seed diagnostics");
    }
  }

  for (const auto& [target, count] : dimension_target_counts) {
    if (count > 1U) {
      add_diagnostic(report, SketchDiagnosticSeverity::Error,
                     SketchDiagnosticKind::DuplicateDrivingDimensionTarget, target,
                     "multiple driving dimensions target the same endpoint pair");
    }
  }

  return report;
}

Result<std::string> serialize_sketch_diagnostic_report_to_json(
    const SketchDiagnosticReport& report) {
  json diagnostics = json::array();
  for (const auto& diagnostic : report.diagnostics()) {
    diagnostics.push_back(json{{"severity", std::string(to_string(diagnostic.severity()))},
                               {"kind", std::string(to_string(diagnostic.kind()))},
                               {"target", diagnostic.target()},
                               {"message", diagnostic.message()}});
  }

  json root{{"schema", "blcad.sketch_diagnostics.debug"},
            {"version", 1},
            {"sketch", report.sketch_id().value()},
            {"warning_count", report.warning_count()},
            {"error_count", report.error_count()},
            {"diagnostics", std::move(diagnostics)}};
  return Result<std::string>::success(root.dump(2));
}

} // namespace blcad
