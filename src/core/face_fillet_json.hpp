#pragma once
#include "blcad/core/advanced_fillet_intent.hpp"
#include <functional>
#include <nlohmann/json.hpp>
namespace blcad::detail {
using FilletFaceWriter = std::function<nlohmann::json(const FaceReference&)>;
using FilletFaceReader =
    std::function<Result<FaceReference>(const nlohmann::json&, PartFeatureInputRole)>;
nlohmann::json face_blend_to_json(const FaceBlendIntent&, const FilletFaceWriter&);
nlohmann::json full_round_to_json(const FullRoundIntent&, const FilletFaceWriter&);
Result<FaceBlendIntent> face_blend_from_json(const nlohmann::json&, const FilletFaceReader&);
Result<FullRoundIntent> full_round_from_json(const nlohmann::json&, const FilletFaceReader&);
nlohmann::json setback_to_json(const FilletSetbackCorner&);
Result<FilletSetbackCorner> setback_from_json(const nlohmann::json&);
} // namespace blcad::detail
