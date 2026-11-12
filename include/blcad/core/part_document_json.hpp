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

// Explicit atomic family replacement through the canonical codec. Retains the
// existing subtractive feature's identity, Body result and executable position;
// incompatible downstream references reject the complete candidate.
[[nodiscard]] Result<PartDocument> replace_part_extrude_with_hole(const PartDocument& document,
                                                                  const HoleFeature& replacement);

// Insert a modifier before a history row atomically, retaining the complete
// suffix. The canonical reader validates ordering, inputs and Body lineage
// together, avoiding an invalid append-then-reorder intermediate document.
[[nodiscard]] Result<PartDocument> insert_part_hole_feature(const PartDocument& document,
                                                            const HoleFeature& feature,
                                                            std::size_t before_index);
[[nodiscard]] Result<PartDocument> insert_part_thread_feature(const PartDocument& document,
                                                              const ThreadFeature& feature,
                                                              std::size_t before_index);

[[nodiscard]] Result<PartDocument> deserialize_part_document_from_json(std::string_view content);
[[nodiscard]] Result<PartDocument> deserialize_part_document_from_json(std::string_view content,
                                                                       PartDocumentReadMode mode);

[[nodiscard]] Result<std::uintmax_t>
write_part_document_json_file(const PartDocument& document, const std::filesystem::path& path);

[[nodiscard]] Result<PartDocument> read_part_document_json_file(const std::filesystem::path& path);
[[nodiscard]] Result<PartDocument> read_part_document_json_file(const std::filesystem::path& path,
                                                                PartDocumentReadMode mode);

} // namespace blcad
