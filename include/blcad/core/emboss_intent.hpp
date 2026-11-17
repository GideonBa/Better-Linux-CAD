#pragma once
#include "blcad/core/part_feature_input_reference.hpp"
#include "blcad/core/sketch_text.hpp"
#include <variant>
namespace blcad {
enum class EmbossOperation { Raise, Recess };
enum class EmbossMapping { Planar, Cylindrical, Conical };
struct PinnedOutlineFont {
  std::string asset;
  std::string sha256;
};
struct EmbossTextSource {
  SketchText text;
  PinnedOutlineFont font;
};
using EmbossSource = std::variant<ProfileRegionReference, EmbossTextSource>;
struct EmbossIntent {
  EmbossSource source;
  std::vector<FaceReference> supports;
  EmbossOperation operation{EmbossOperation::Raise};
  ParameterId depth;
  EmbossMapping mapping{EmbossMapping::Planar};
  // Developed chart coordinates, millimeters. Cone v is meridian arclength.
  Point2 anchor_mm;
  // Physical chart seam around the carrier axis, in degrees; never normalized on save.
  double seam_degrees{};
  std::optional<ParameterId> orientation;
};
[[nodiscard]] std::optional<PinnedOutlineFont> bundled_outline_font(std::string_view family);
[[nodiscard]] SketchId emboss_source_sketch(const EmbossIntent&);
} // namespace blcad
