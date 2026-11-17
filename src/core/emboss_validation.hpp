#pragma once
#include "blcad/core/emboss_intent.hpp"
namespace blcad::detail {
Result<std::size_t> validate_emboss_structure(const EmbossIntent&, FeatureId);
Result<std::size_t> validate_emboss_parameters(const PartDocument&, const EmbossIntent&, FeatureId);
Result<std::size_t> validate_emboss_inputs(const PartDocument&, const EmbossIntent&, BodyId,
                                           FeatureId);
std::vector<ParameterId> emboss_parameters(const EmbossIntent&);
std::vector<std::string> emboss_inputs(const EmbossIntent&);
} // namespace blcad::detail
