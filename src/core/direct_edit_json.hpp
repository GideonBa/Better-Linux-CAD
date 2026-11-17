#pragma once
#include "blcad/core/direct_edit_feature.hpp"
#include <functional>
#include <nlohmann/json.hpp>
namespace blcad::detail {
using DirectEditFaceWriter = std::function<nlohmann::json(const FaceReference&)>;
using DirectEditFaceReader =
    std::function<Result<FaceReference>(const nlohmann::json&, PartFeatureInputRole)>;
nlohmann::json direct_edit_to_json(const DirectEditFeature&, const DirectEditFaceWriter&);
Result<DirectEditFeature> direct_edit_from_json(const nlohmann::json&, const DirectEditFaceReader&);
} // namespace blcad::detail
