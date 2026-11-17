#pragma once
#include "emboss_mapping.hpp"
#include "rib_native.hpp"
namespace blcad::geometry::detail {
RibNative emboss_tools(const std::vector<EmbossOutlineLoop>&, const EmbossChart&,
                       const std::vector<TopoDS_Face>&, const FeatureId&);
}
