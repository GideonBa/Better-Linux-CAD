#pragma once

#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace blcad {

[[nodiscard]] Result<std::string> serialize_project_to_json(const Project& project);

[[nodiscard]] Result<Project> deserialize_project_from_json(std::string_view content);

[[nodiscard]] Result<std::uintmax_t> write_project_json_file(const Project& project,
                                                            const std::filesystem::path& path);

[[nodiscard]] Result<Project> read_project_json_file(const std::filesystem::path& path);

} // namespace blcad
