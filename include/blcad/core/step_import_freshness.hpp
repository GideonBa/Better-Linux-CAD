#pragma once

#include "blcad/core/step_import_source.hpp"
#include <filesystem>
#include <optional>

namespace blcad {
class PartDocument;
class Project;
enum class StepImportFreshness { Available, Missing, Changed, Unreadable };
[[nodiscard]] std::string_view to_string(StepImportFreshness state) noexcept;
[[nodiscard]] Result<std::string> step_import_sha256(std::string_view bytes);

class StepImportAssetInspection {
public:
  [[nodiscard]] const std::filesystem::path& root_directory() const noexcept {
    return root_;
  }
  [[nodiscard]] const std::string& project_relative_asset_path() const noexcept {
    return path_;
  }
  [[nodiscard]] StepImportFreshness state() const noexcept {
    return state_;
  }
  [[nodiscard]] const std::optional<std::string>& observed_sha256_digest() const noexcept {
    return digest_;
  }
  [[nodiscard]] const std::string& diagnostic() const noexcept {
    return diagnostic_;
  }

private:
  friend Result<StepImportAssetInspection> inspect_step_import_asset(const std::filesystem::path&,
                                                                     std::string_view);
  StepImportAssetInspection(std::filesystem::path root, std::string path, StepImportFreshness state,
                            std::optional<std::string> digest, std::string diagnostic)
      : root_(std::move(root)), path_(std::move(path)), state_(state), digest_(std::move(digest)),
        diagnostic_(std::move(diagnostic)) {}
  std::filesystem::path root_;
  std::string path_;
  StepImportFreshness state_;
  std::optional<std::string> digest_;
  std::string diagnostic_;
};

class StepImportFreshnessReport {
public:
  [[nodiscard]] const StepImportSource& source() const noexcept {
    return source_;
  }
  [[nodiscard]] const std::filesystem::path& root_directory() const noexcept {
    return asset_.root_directory();
  }
  [[nodiscard]] const std::optional<std::string>& observed_sha256_digest() const noexcept {
    return asset_.observed_sha256_digest();
  }
  [[nodiscard]] StepImportFreshness state() const noexcept {
    const auto& observed = asset_.observed_sha256_digest();
    return asset_.state() == StepImportFreshness::Available &&
                   (!observed || *observed != source_.sha256_digest())
               ? StepImportFreshness::Changed
               : asset_.state();
  }
  [[nodiscard]] std::string diagnostic() const {
    return state() == StepImportFreshness::Changed
               ? "STEP source content differs from its registered SHA-256 digest"
               : asset_.diagnostic();
  }

private:
  friend Result<StepImportFreshnessReport> inspect_step_import_source(const StepImportSource&,
                                                                      const std::filesystem::path&);
  StepImportFreshnessReport(StepImportSource source, StepImportAssetInspection asset)
      : source_(std::move(source)), asset_(std::move(asset)) {}
  StepImportSource source_;
  StepImportAssetInspection asset_;
};

// Read-only filesystem access. All path components and the root must be real directories/files,
// never symlinks. The opened file must remain the same regular file throughout inspection.
[[nodiscard]] Result<StepImportAssetInspection>
inspect_step_import_asset(const std::filesystem::path& root_directory,
                          std::string_view relative_path);
[[nodiscard]] Result<StepImportFreshnessReport>
inspect_step_import_source(const StepImportSource& source,
                           const std::filesystem::path& root_directory);

// Return a runtime-only byte snapshot from the same verified read used by inspection. The source
// must be available, unchanged throughout the read, and match its accepted SHA-256 digest. No path
// is exposed for a later unchecked parser read, and neither the source nor its host is modified.
[[nodiscard]] Result<std::string>
read_verified_step_import_source(const StepImportSource& source,
                                 const std::filesystem::path& root_directory);

// Re-read the source and compare both accepted intent and the previously observed digest/root.
// Missing, unreadable, changed-since-preview or stale-intent results cannot mutate the registry.
[[nodiscard]] Result<std::size_t>
refresh_step_import_source(PartDocument& document, const std::filesystem::path& root_directory,
                           const StepImportFreshnessReport& report);
[[nodiscard]] Result<std::size_t>
refresh_step_import_source(Project& project, const std::filesystem::path& root_directory,
                           const StepImportFreshnessReport& report);
} // namespace blcad
