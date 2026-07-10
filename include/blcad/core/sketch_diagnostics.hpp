#pragma once

#include "blcad/core/result.hpp"
#include "blcad/core/sketch.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace blcad {

enum class SketchDiagnosticSeverity { Info, Warning, Error };
[[nodiscard]] std::string_view to_string(SketchDiagnosticSeverity severity) noexcept;

enum class SketchDiagnosticKind {
  UnconstrainedLineEndpoint,
  FreeSplineControlPoint,
  ProfileWithoutDrivingDimensions,
  ConflictingHorizontalVerticalConstraints,
  RedundantOrientationConstraint,
  DuplicateFixedEndpoint,
  DuplicateDrivingDimensionTarget
};
[[nodiscard]] std::string_view to_string(SketchDiagnosticKind kind) noexcept;

class SketchConstraintDiagnostic {
public:
  SketchConstraintDiagnostic(SketchDiagnosticSeverity severity, SketchDiagnosticKind kind,
                             std::string target, std::string message);

  [[nodiscard]] SketchDiagnosticSeverity severity() const noexcept;
  [[nodiscard]] SketchDiagnosticKind kind() const noexcept;
  [[nodiscard]] const std::string& target() const noexcept;
  [[nodiscard]] const std::string& message() const noexcept;

private:
  SketchDiagnosticSeverity severity_;
  SketchDiagnosticKind kind_;
  std::string target_;
  std::string message_;
};

class SketchDiagnosticReport {
public:
  explicit SketchDiagnosticReport(SketchId sketch_id);

  [[nodiscard]] const SketchId& sketch_id() const noexcept;
  [[nodiscard]] const std::vector<SketchConstraintDiagnostic>& diagnostics() const noexcept;
  [[nodiscard]] std::size_t warning_count() const noexcept;
  [[nodiscard]] std::size_t error_count() const noexcept;
  [[nodiscard]] bool has_errors() const noexcept;

  void add(SketchConstraintDiagnostic diagnostic);

private:
  SketchId sketch_id_;
  std::vector<SketchConstraintDiagnostic> diagnostics_;
};

class SketchConstraintDiagnostics {
public:
  [[nodiscard]] SketchDiagnosticReport analyze(const Sketch& sketch) const;
};

[[nodiscard]] Result<std::string> serialize_sketch_diagnostic_report_to_json(
    const SketchDiagnosticReport& report);

} // namespace blcad
