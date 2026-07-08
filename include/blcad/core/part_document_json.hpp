#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"

#include <string>
#include <string_view>

namespace blcad {

[[nodiscard]] Result<std::string> serialize_part_document_to_json(const PartDocument& document);

[[nodiscard]] Result<PartDocument> deserialize_part_document_from_json(std::string_view content);

} // namespace blcad
