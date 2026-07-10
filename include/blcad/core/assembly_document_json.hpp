#pragma once

#include "blcad/core/assembly_document.hpp"
#include "blcad/core/result.hpp"

#include <string>
#include <string_view>

namespace blcad {

[[nodiscard]] Result<std::string>
serialize_assembly_document_to_json(const AssemblyDocument& document);

[[nodiscard]] Result<AssemblyDocument>
deserialize_assembly_document_from_json(std::string_view content);

} // namespace blcad
