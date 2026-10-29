#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace blcad {
enum class StepImportMode { Reference, EditableBody };
[[nodiscard]] std::string_view to_string(StepImportMode mode) noexcept;
[[nodiscard]] Result<std::string> canonical_step_import_asset_path(std::string_view path);

// External source intent only. Geometry, filesystem state and STEP payload stay derived.
class StepImportSource {
public:
  [[nodiscard]] static Result<StepImportSource> create(StepImportSourceId id, std::string name,
                                                       std::string project_relative_asset_path,
                                                       std::string sha256_digest,
                                                       StepImportMode mode);
  [[nodiscard]] const StepImportSourceId& id() const noexcept {
    return id_;
  }
  [[nodiscard]] const std::string& name() const noexcept {
    return name_;
  }
  [[nodiscard]] const std::string& project_relative_asset_path() const noexcept {
    return path_;
  }
  [[nodiscard]] const std::string& sha256_digest() const noexcept {
    return digest_;
  }
  [[nodiscard]] StepImportMode mode() const noexcept {
    return mode_;
  }
  [[nodiscard]] std::string node_id() const {
    return "step-import-source:" + id_.value();
  }
  [[nodiscard]] Result<StepImportSource> with_sha256_digest(std::string digest) const;
  friend bool operator==(const StepImportSource&, const StepImportSource&) = default;

private:
  StepImportSource(StepImportSourceId id, std::string name, std::string path, std::string digest,
                   StepImportMode mode);
  StepImportSourceId id_;
  std::string name_, path_, digest_;
  StepImportMode mode_;
};

// Each Part and Project owns one independent registry; no unchecked mutable access.
class StepImportSourceRegistry {
public:
  [[nodiscard]] const std::vector<StepImportSource>& sources() const noexcept {
    return sources_;
  }
  [[nodiscard]] const StepImportSource* find(StepImportSourceId id) const noexcept;
  [[nodiscard]] Result<std::size_t> add(StepImportSource source);
  [[nodiscard]] Result<std::size_t> remove(StepImportSourceId id);
  [[nodiscard]] Result<std::size_t> remove(const StepImportSource& expected);
  [[nodiscard]] Result<std::size_t> refresh(const StepImportSource& expected, std::string digest);

private:
  std::vector<StepImportSource> sources_;
};
} // namespace blcad
