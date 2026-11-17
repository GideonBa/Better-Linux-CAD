#pragma once
#include "blcad/core/result.hpp"
#include <cstddef>
namespace blcad {
class PartDocument;
class FilletFeature;
} // namespace blcad
namespace blcad::detail {
[[nodiscard]] Result<std::size_t> validate_fillet_parameters(const PartDocument&,
                                                             const FilletFeature&);
}
