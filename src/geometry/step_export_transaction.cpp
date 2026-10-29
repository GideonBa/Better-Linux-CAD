#include "step_export_transaction.hpp"

#include "blcad/core/part_document.hpp"
#include "blcad/core/project.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry::detail {
namespace {
Error invalid(std::string message) {
  return Error::export_error("geometry.step_export", std::move(message));
}

Result<bool> aliases(const std::filesystem::path& first, const std::filesystem::path& second) {
  std::error_code error;
  const auto first_absolute = std::filesystem::absolute(first, error).lexically_normal();
  if (error)
    return Result<bool>::failure(invalid("could not resolve protected STEP asset path"));
  const auto second_absolute = std::filesystem::absolute(second, error).lexically_normal();
  if (error)
    return Result<bool>::failure(invalid("could not resolve STEP output path"));
  if (first_absolute == second_absolute)
    return Result<bool>::success(true);
  if (std::filesystem::equivalent(first_absolute, second_absolute, error) && !error)
    return Result<bool>::success(true);
  error.clear();
  const auto first_canonical = std::filesystem::weakly_canonical(first_absolute, error);
  if (error)
    return Result<bool>::failure(invalid("could not canonicalize protected STEP asset path"));
  const auto second_canonical = std::filesystem::weakly_canonical(second_absolute, error);
  if (error)
    return Result<bool>::failure(invalid("could not canonicalize STEP output path"));
  return Result<bool>::success(first_canonical == second_canonical);
}
} // namespace

struct StepExportTransaction::Impl {
  StepImportTopologyContext* context{};
  std::vector<std::filesystem::path> sources;
  std::filesystem::path target;
  std::filesystem::path writer;
  std::string target_name;
  std::string temporary_name;
  int directory{-1};
  int temporary{-1};
  bool committed{};

  ~Impl() {
    if (directory >= 0 && temporary >= 0 && !temporary_name.empty() && !committed) {
      struct stat owned {
      }, current{};
      if (::fstat(temporary, &owned) == 0 &&
          ::fstatat(directory, temporary_name.c_str(), &current, AT_SYMLINK_NOFOLLOW) == 0 &&
          owned.st_dev == current.st_dev && owned.st_ino == current.st_ino)
        ::unlinkat(directory, temporary_name.c_str(), 0);
    }
    if (temporary >= 0)
      ::close(temporary);
    if (directory >= 0)
      ::close(directory);
  }

  Result<std::size_t> protect(const std::filesystem::path& output) const {
    for (const auto& source : sources) {
      auto same = aliases(source, output);
      if (!same)
        return Result<std::size_t>::failure(same.error());
      if (same.value())
        return Result<std::size_t>::failure(
            invalid("STEP output must not overwrite or alias a registered source asset"));
    }
    return Result<std::size_t>::success(sources.size());
  }

  static Result<std::unique_ptr<Impl>> create(std::vector<StepImportSource> registered,
                                              StepImportTopologyContext* context,
                                              const std::filesystem::path& output) {
    if (output.empty())
      return Result<std::unique_ptr<Impl>>::failure(
          invalid("STEP export output path must not be empty"));
    if (!registered.empty() && (context == nullptr || context->root_directory().empty()))
      return Result<std::unique_ptr<Impl>>::failure(
          invalid("document-aware STEP export with registered assets requires an explicit "
                  "source-root context"));
    try {
      auto result = std::make_unique<Impl>();
      result->context = context;
      result->target = std::filesystem::absolute(output).lexically_normal();
      result->writer = result->target;
      if (registered.empty() && context == nullptr)
        return Result<std::unique_ptr<Impl>>::success(std::move(result));
      for (const auto& source : registered)
        result->sources.push_back(context->root_directory() / source.project_relative_asset_path());
      auto protected_output = result->protect(result->target);
      if (!protected_output)
        return Result<std::unique_ptr<Impl>>::failure(protected_output.error());

      result->directory =
          ::open(result->target.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
      if (result->directory < 0)
        return Result<std::unique_ptr<Impl>>::failure(
            invalid("could not open STEP output directory"));
      result->target_name = result->target.filename().string();
      const auto anchored_directory =
          std::filesystem::path("/proc/self/fd") / std::to_string(result->directory);
      // Reserve and retain the file descriptor: the kernel writer opens this
      // inode through /proc/self/fd, never a replaceable temporary pathname.
      result->temporary_name = ".blcad-step-export-XXXXXX";
      std::string temporary = (anchored_directory / result->temporary_name).string();
      result->temporary = ::mkstemp(temporary.data());
      if (result->temporary < 0)
        return Result<std::unique_ptr<Impl>>::failure(
            invalid("could not reserve temporary STEP output"));
      std::copy(temporary.end() - static_cast<std::ptrdiff_t>(result->temporary_name.size()),
                temporary.end(), result->temporary_name.begin());
      result->writer = std::filesystem::path("/proc/self/fd") / std::to_string(result->temporary);
      return Result<std::unique_ptr<Impl>>::success(std::move(result));
    } catch (const std::exception& failure) {
      return Result<std::unique_ptr<Impl>>::failure(invalid(failure.what()));
    }
  }
};

StepExportTransaction::StepExportTransaction(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
StepExportTransaction::~StepExportTransaction() = default;
StepExportTransaction::StepExportTransaction(StepExportTransaction&&) noexcept = default;
StepExportTransaction& StepExportTransaction::operator=(StepExportTransaction&&) noexcept = default;

Result<StepExportTransaction> StepExportTransaction::create(const PartDocument& document,
                                                            StepImportTopologyContext* context,
                                                            const std::filesystem::path& output) {
  auto created = Impl::create(document.step_import_sources(), context, output);
  if (!created)
    return Result<StepExportTransaction>::failure(created.error());
  return Result<StepExportTransaction>::success(StepExportTransaction(std::move(created.value())));
}

Result<StepExportTransaction> StepExportTransaction::create(const Project& project,
                                                            StepImportTopologyContext* context,
                                                            const std::filesystem::path& output) {
  std::vector<StepImportSource> sources = project.step_import_sources();
  for (const auto& part : project.part_documents())
    sources.insert(sources.end(), part.step_import_sources().begin(),
                   part.step_import_sources().end());
  auto created = Impl::create(std::move(sources), context, output);
  if (!created)
    return Result<StepExportTransaction>::failure(created.error());
  return Result<StepExportTransaction>::success(StepExportTransaction(std::move(created.value())));
}

const std::filesystem::path& StepExportTransaction::writer_path() const noexcept {
  return impl_->writer;
}

Result<std::size_t> StepExportTransaction::commit() {
  if (impl_->committed)
    return Result<std::size_t>::failure(invalid("STEP output transaction has already committed"));
  if (impl_->context) {
    auto fresh = impl_->context->validate_sources();
    if (!fresh)
      return Result<std::size_t>::failure(fresh.error());
  }
  if (impl_->directory < 0) {
    std::error_code error;
    const auto size = std::filesystem::file_size(impl_->target, error);
    if (error || size == 0)
      return Result<std::size_t>::failure(invalid("STEP output is unavailable or empty"));
    impl_->committed = true;
    return Result<std::size_t>::success(static_cast<std::size_t>(size));
  }
  struct stat descriptor {
  }, path{};
  if (::fstat(impl_->temporary, &descriptor) != 0 ||
      ::fstatat(impl_->directory, impl_->temporary_name.c_str(), &path, AT_SYMLINK_NOFOLLOW) != 0 ||
      !S_ISREG(descriptor.st_mode) || !S_ISREG(path.st_mode) || descriptor.st_dev != path.st_dev ||
      descriptor.st_ino != path.st_ino || descriptor.st_size <= 0)
    return Result<std::size_t>::failure(invalid("temporary STEP output changed or is empty"));
  const auto anchored_target = std::filesystem::path("/proc/self/fd") /
                               std::to_string(impl_->directory) / impl_->target_name;
  auto protected_output = impl_->protect(anchored_target);
  if (!protected_output)
    return Result<std::size_t>::failure(protected_output.error());
  if (::renameat(impl_->directory, impl_->temporary_name.c_str(), impl_->directory,
                 impl_->target_name.c_str()) != 0)
    return Result<std::size_t>::failure(invalid("could not atomically publish STEP output"));
  impl_->committed = true;
  return Result<std::size_t>::success(static_cast<std::size_t>(descriptor.st_size));
}

} // namespace blcad::geometry::detail
