#pragma once
#include "blcad/core/sketch_driven_intent.hpp"
#include <functional>
#include <nlohmann/json.hpp>
namespace blcad::detail {
using SketchDrivenFaceWriter = std::function<nlohmann::json(const FaceReference&)>;
using SketchDrivenFaceReader =
    std::function<Result<FaceReference>(const nlohmann::json&, PartFeatureInputRole)>;
nlohmann::json sketch_driven_to_json(const SketchDrivenFeature&, const SketchDrivenFaceWriter&);
Result<SketchDrivenFeature> sketch_driven_from_json(const nlohmann::json&,
                                                    const SketchDrivenFaceReader&);
} // namespace blcad::detail
