#pragma once

#include "blcad/core/part_document.hpp"

#include <string_view>

namespace blcad {

// Decode one replacement with the canonical persisted record parser, then use
// the typed atomic update authority. Dependency-derived history may reorder;
// an explicit authored history retains its validated positions.
[[nodiscard]] Result<PartDocument> replace_part_pattern_from_json(const PartDocument& document,
                                                                  std::string_view record);
[[nodiscard]] Result<PartDocument> replace_part_mirror_from_json(const PartDocument& document,
                                                                 std::string_view record);

} // namespace blcad
