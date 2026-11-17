#pragma once
#include "blcad/core/part_feature_input_reference.hpp"
#include <array>
#include <optional>
#include <string>
#include <variant>
#include <vector>
namespace blcad {
// Stations measure true normalized length of an oriented, ordered contour.
// Start and End remain semantic endpoints if the contour length changes.
enum class FilletStationLocation { Fraction, Start, End };
enum class FilletRadiusInterpolation { Linear, MonotoneCubic };
enum class FilletPropagation { Explicit, TangentChain };
struct FilletRadiusStation {
  std::string id;
  FilletStationLocation location = FilletStationLocation::Fraction;
  double fraction = 0;
  ParameterId radius;
  [[nodiscard]] double normalized_length() const noexcept;
};
struct FilletOrientedEdge {
  EdgeReference edge;
  bool reversed = false;
};
struct FilletEdgeGroup {
  std::string id, name;
  std::vector<FilletOrientedEdge> edges;
  FilletPropagation propagation = FilletPropagation::Explicit;
  FilletRadiusInterpolation interpolation = FilletRadiusInterpolation::Linear;
  std::vector<FilletRadiusStation> stations;
  bool station_basis_reversed = false;
};
struct FilletSetbackCorner {
  std::string id;
  std::array<std::string, 3> groups;
  ParameterId distance;
};
struct VariableFilletIntent {
  std::vector<FilletEdgeGroup> groups;
  std::optional<FilletSetbackCorner> setback = {};
};
enum class FilletContinuity { Tangent, Curvature };
enum class FaceBlendSizeKind { Radius, ContactSetback };
enum class FilletSide { First, Second };
struct FilletFaceSet {
  std::string id, name;
  std::vector<FaceReference> faces;
};
struct FilletHoldLine {
  std::string id;
  FilletSide side;
  PathCurveId path;
};
struct FaceBlendIntent {
  FilletFaceSet first, second;
  FilletContinuity continuity = FilletContinuity::Tangent;
  FaceBlendSizeKind size_kind = FaceBlendSizeKind::Radius;
  ParameterId size;
  std::vector<FilletHoldLine> hold_lines;
};
struct FullRoundIntent {
  FilletFaceSet first, second, middle;
  FilletContinuity continuity = FilletContinuity::Tangent;
  std::vector<FilletHoldLine> hold_lines;
};
[[nodiscard]] Result<std::size_t> validate_face_blend_intent(const FaceBlendIntent&);
[[nodiscard]] Result<std::size_t> validate_full_round_intent(const FullRoundIntent&);

struct AdvancedFilletIntent {
  std::variant<VariableFilletIntent, FaceBlendIntent, FullRoundIntent> definition;
};
[[nodiscard]] Result<std::size_t> validate_variable_fillet_intent(const VariableFilletIntent&);
// Reverses both semantic member orientation and the authored law, retaining all IDs.
[[nodiscard]] FilletEdgeGroup reverse_fillet_group(FilletEdgeGroup);
[[nodiscard]] double fillet_chain_fraction(const FilletEdgeGroup&,
                                           const FilletRadiusStation&) noexcept;
} // namespace blcad
