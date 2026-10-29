#pragma once

#include "blcad/core/result.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>

namespace blcad {
class PartDocument;
class Project;
} // namespace blcad

namespace blcad::geometry {
class StepImportTopologyContext;

namespace detail {
// Document-aware STEP output guard and commit boundary. Import-aware writes go
// to a reserved inode and replace the destination only after source revalidation.
class StepExportTransaction {
public:
  ~StepExportTransaction();
  StepExportTransaction(StepExportTransaction&&) noexcept;
  StepExportTransaction& operator=(StepExportTransaction&&) noexcept;
  StepExportTransaction(const StepExportTransaction&) = delete;
  StepExportTransaction& operator=(const StepExportTransaction&) = delete;

  [[nodiscard]] static Result<StepExportTransaction> create(const PartDocument& document,
                                                            StepImportTopologyContext* context,
                                                            const std::filesystem::path& output);
  [[nodiscard]] static Result<StepExportTransaction> create(const Project& project,
                                                            StepImportTopologyContext* context,
                                                            const std::filesystem::path& output);
  [[nodiscard]] const std::filesystem::path& writer_path() const noexcept;
  [[nodiscard]] Result<std::size_t> commit();

private:
  struct Impl;
  explicit StepExportTransaction(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};
} // namespace detail
} // namespace blcad::geometry
