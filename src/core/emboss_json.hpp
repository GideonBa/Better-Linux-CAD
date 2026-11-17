#pragma once
#include "sketch_driven_json.hpp"
namespace blcad::detail {
nlohmann::json emboss_to_json(const SketchDrivenFeature&, const SketchDrivenFaceWriter&);
Result<SketchDrivenFeature> emboss_from_json(const nlohmann::json&, const SketchDrivenFaceReader&);
} // namespace blcad::detail
