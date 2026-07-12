#pragma once

#include "blcad/core/assembly_joint.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_contact_analyzer.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"

#include <cstddef>
#include <vector>

namespace blcad::geometry {

inline constexpr std::size_t kAssemblyRevoluteSweepMaximumSampleCount = 1001U;

enum class AssemblyRevoluteSweepJointScope {
  RootAssemblyLocal,
  ProjectCrossHierarchy,
};

struct AssemblyRevoluteSweepJointReference {
  AssemblyRevoluteSweepJointScope scope = AssemblyRevoluteSweepJointScope::RootAssemblyLocal;
  AssemblyJointId joint;

  friend bool operator==(const AssemblyRevoluteSweepJointReference&,
                         const AssemblyRevoluteSweepJointReference&) = default;
};

struct AssemblyRevoluteSweepRequest {
  AssemblyRevoluteSweepJointReference joint;
  Quantity start_coordinate;
  Quantity end_coordinate;
  std::size_t sample_count = 0U;
};

struct AssemblyRevoluteSweepAnalysisOptions {
  AssemblyContactAnalysisOptions contact;
  AssemblyRigidBodySolverOptions motion_solver;
};

struct AssemblyRevoluteSweepSample {
  std::size_t sample_index = 0U;
  double coordinate_deg = 0.0;
  std::size_t applied_transform_count = 0U;
  AssemblyContactAnalysis contact_analysis;

  friend bool operator==(const AssemblyRevoluteSweepSample&,
                         const AssemblyRevoluteSweepSample&) = default;
};

struct AssemblyRevoluteSweepAnalysis {
  AssemblyRevoluteSweepJointReference joint;
  double start_coordinate_deg = 0.0;
  double end_coordinate_deg = 0.0;
  std::vector<AssemblyRevoluteSweepSample> samples;

  friend bool operator==(const AssemblyRevoluteSweepAnalysis&,
                         const AssemblyRevoluteSweepAnalysis&) = default;
};

// Read-only bounded sampled sweep. Every sample starts from a fresh copy of the
// same source Project, drives one currently supported Revolute motion contract,
// atomically applies that result to the copy, and classifies posed contact. This
// is deterministic discrete sampling and is not continuous collision detection.
class AssemblyRevoluteSweepAnalyzer {
public:
  [[nodiscard]] Result<AssemblyRevoluteSweepAnalysis>
  analyze(const Project& project, const AssemblyRevoluteSweepRequest& request,
          AssemblyRevoluteSweepAnalysisOptions options = {}) const;
};

} // namespace blcad::geometry
