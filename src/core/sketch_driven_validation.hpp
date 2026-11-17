#pragma once
#include "blcad/core/part_document.hpp"
namespace blcad::detail {
Result<std::size_t> validate_sketch_driven_parameters(const PartDocument&,
                                                      const SketchDrivenFeature&);
Result<std::size_t> validate_sketch_driven_inputs(const PartDocument&, const SketchDrivenFeature&);
} // namespace blcad::detail
