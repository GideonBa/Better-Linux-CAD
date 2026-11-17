#pragma once
#include "blcad/core/emboss_intent.hpp"
#include <Geom2d_Curve.hxx>
namespace blcad::geometry::detail {
struct EmbossOutlineLoop {
  std::string role;
  std::vector<Handle(Geom2d_Curve)> curves;
};
std::vector<EmbossOutlineLoop> emboss_text_outlines(const PartDocument&, const EmbossTextSource&);
} // namespace blcad::geometry::detail
