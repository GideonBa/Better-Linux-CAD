#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace blcad {

// Repair loading retains only missing sources of existing projected Sketch
// references and face-derived workplanes. All other validation remains strict.
enum class PartDocumentReadMode { Strict, ReferenceRepair };

[[nodiscard]] Result<std::string> serialize_part_document_to_json(const PartDocument& document);

[[nodiscard]] Result<PartDocument> deserialize_part_document_from_json(std::string_view content);
[[nodiscard]] Result<PartDocument> deserialize_part_document_from_json(std::string_view content,
                                                                       PartDocumentReadMode mode);

[[nodiscard]] Result<std::uintmax_t>
write_part_document_json_file(const PartDocument& document, const std::filesystem::path& path);

[[nodiscard]] Result<PartDocument> read_part_document_json_file(const std::filesystem::path& path);
[[nodiscard]] Result<PartDocument> read_part_document_json_file(const std::filesystem::path& path,
                                                                PartDocumentReadMode mode);

} // namespace blcad
