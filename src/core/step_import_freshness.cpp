#include "blcad/core/step_import_freshness.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/core/project.hpp"
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <new>
#include <openssl/evp.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace blcad {
namespace {
Error invalid(std::string message) {
  return Error::validation("step_import_freshness", std::move(message));
}
Error crypto_error() {
  return Error::internal("step_import_freshness", "SHA-256 computation failed");
}
class FileDescriptor {
public:
  explicit FileDescriptor(int value = -1) : value_(value) {}
  ~FileDescriptor() {
    const auto saved = errno;
    if (value_ >= 0)
      ::close(value_);
    errno = saved;
  }
  FileDescriptor(const FileDescriptor&) = delete;
  FileDescriptor& operator=(const FileDescriptor&) = delete;
  FileDescriptor(FileDescriptor&& other) noexcept : value_(std::exchange(other.value_, -1)) {}
  FileDescriptor& operator=(FileDescriptor&& other) noexcept {
    if (this != &other) {
      if (value_ >= 0)
        ::close(value_);
      value_ = std::exchange(other.value_, -1);
    }
    return *this;
  }
  int get() const {
    return value_;
  }

private:
  int value_;
};
FileDescriptor open_asset(const std::filesystem::path& root, const std::string& path) {
  FileDescriptor directory(
      ::open(root.root_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (directory.get() < 0)
    return directory;
  // Walk the absolute root too: swapping an ancestor to a symlink after canonical()
  // must never redirect the asset read outside the selected directory.
  for (const auto& component : root.relative_path()) {
    FileDescriptor next(::openat(directory.get(), component.c_str(),
                                 O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (next.get() < 0)
      return next;
    directory = std::move(next);
  }
  std::size_t offset = 0;
  while (true) {
    const auto end = path.find('/', offset);
    const auto name =
        path.substr(offset, end == std::string::npos ? std::string::npos : end - offset);
    const bool final = end == std::string::npos;
    FileDescriptor next(
        ::openat(directory.get(), name.c_str(),
                 O_RDONLY | O_CLOEXEC | O_NOFOLLOW | (final ? O_NONBLOCK : O_DIRECTORY)));
    if (next.get() < 0 || final)
      return next;
    directory = std::move(next);
    offset = end + 1;
  }
}
bool unchanged(const struct stat& a, const struct stat& b) {
  return a.st_dev == b.st_dev && a.st_ino == b.st_ino && a.st_mode == b.st_mode &&
         a.st_size == b.st_size && a.st_mtim.tv_sec == b.st_mtim.tv_sec &&
         a.st_mtim.tv_nsec == b.st_mtim.tv_nsec && a.st_ctim.tv_sec == b.st_ctim.tv_sec &&
         a.st_ctim.tv_nsec == b.st_ctim.tv_nsec;
}
using DigestContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
DigestContext digest_context() {
  return DigestContext(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
}
Result<std::string> finish_digest(EVP_MD_CTX* context) {
  std::array<unsigned char, EVP_MAX_MD_SIZE> bytes{};
  unsigned int count = 0;
  if (EVP_DigestFinal_ex(context, bytes.data(), &count) != 1 || count != 32)
    return Result<std::string>::failure(crypto_error());
  constexpr char hex[] = "0123456789abcdef";
  std::string result;
  result.reserve(64);
  for (unsigned int i = 0; i < count; ++i) {
    result += hex[bytes[i] >> 4];
    result += hex[bytes[i] & 15];
  }
  return Result<std::string>::success(std::move(result));
}
struct AssetRead {
  std::filesystem::path root;
  std::string path;
  StepImportFreshness state;
  std::optional<std::string> digest;
  std::string diagnostic;
  std::string bytes;
};
Result<AssetRead> read_asset(const std::filesystem::path& root_directory,
                             std::string_view relative_path, bool retain_bytes) {
  try {
    auto path = canonical_step_import_asset_path(relative_path);
    if (!path)
      return Result<AssetRead>::failure(path.error());
    std::error_code error;
    auto root = std::filesystem::absolute(root_directory, error).lexically_normal();
    while (root != root.root_path() && root.filename().empty())
      root = root.parent_path();
    const auto report = [&](StepImportFreshness state, std::string diagnostic,
                            std::optional<std::string> digest = {}, std::string bytes = {}) {
      return Result<AssetRead>::success(AssetRead{root, path.value(), state, std::move(digest),
                                                  std::move(diagnostic), std::move(bytes)});
    };
    if (root_directory.empty() || error)
      return report(StepImportFreshness::Unreadable,
                    "STEP asset root must be an existing real directory");
    const auto canonical_root = std::filesystem::canonical(root, error);
    if (error || canonical_root != root)
      return report(StepImportFreshness::Unreadable,
                    "STEP asset root must exist and must not contain symlinks");
    FileDescriptor input = open_asset(root, path.value());
    if (input.get() < 0) {
      const auto code = errno;
      return report(code == ENOENT ? StepImportFreshness::Missing : StepImportFreshness::Unreadable,
                    "STEP asset cannot be opened without following symlinks: " +
                        std::error_code(code, std::generic_category()).message());
    }
    struct stat before {};
    if (::fstat(input.get(), &before) != 0 || !S_ISREG(before.st_mode) ||
        !(before.st_mode & (S_IRUSR | S_IRGRP | S_IROTH)))
      return report(StepImportFreshness::Unreadable, "STEP asset must be a readable regular file");
    auto context = digest_context();
    if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1)
      return Result<AssetRead>::failure(crypto_error());
    std::string bytes;
    if (retain_bytes && before.st_size >= 0) {
      if (static_cast<std::uintmax_t>(before.st_size) > bytes.max_size())
        return Result<AssetRead>::failure(
            Error::internal("step_import_freshness", "STEP source byte snapshot is too large"));
      bytes.reserve(static_cast<std::size_t>(before.st_size));
    }
    std::array<char, 65536> buffer{};
    std::uintmax_t read_bytes = 0;
    while (true) {
      const auto count = ::read(input.get(), buffer.data(), buffer.size());
      if (count < 0 && errno == EINTR)
        continue;
      if (count < 0)
        return report(StepImportFreshness::Unreadable, "STEP asset read failed");
      if (count == 0)
        break;
      read_bytes += static_cast<std::uintmax_t>(count);
      if (before.st_size < 0 || read_bytes > static_cast<std::uintmax_t>(before.st_size))
        return report(StepImportFreshness::Unreadable,
                      "STEP asset grew while being inspected; inspect again");
      if (EVP_DigestUpdate(context.get(), buffer.data(), static_cast<std::size_t>(count)) != 1)
        return Result<AssetRead>::failure(crypto_error());
      if (retain_bytes)
        bytes.append(buffer.data(), static_cast<std::size_t>(count));
    }
    struct stat after {
    }, reopened{};
    FileDescriptor current = open_asset(root, path.value());
    if (::fstat(input.get(), &after) != 0 || current.get() < 0 ||
        ::fstat(current.get(), &reopened) != 0 || !unchanged(before, after) ||
        !unchanged(after, reopened) || before.st_size < 0 ||
        read_bytes != static_cast<std::uintmax_t>(before.st_size))
      return report(StepImportFreshness::Unreadable,
                    "STEP asset changed while being inspected; inspect again");
    auto digest = finish_digest(context.get());
    if (!digest)
      return Result<AssetRead>::failure(digest.error());
    return report(StepImportFreshness::Available, "STEP source is available and readable",
                  std::move(digest.value()), std::move(bytes));
  } catch (const std::bad_alloc&) {
    return Result<AssetRead>::failure(
        Error::internal("step_import_freshness", "STEP source byte snapshot allocation failed"));
  } catch (const std::length_error&) {
    return Result<AssetRead>::failure(
        Error::internal("step_import_freshness", "STEP source byte snapshot is too large"));
  }
}
template <typename Host>
Result<std::size_t> refresh(Host& host, const std::filesystem::path& root,
                            const StepImportFreshnessReport& report) {
  const auto* current = host.find_step_import_source(report.source().id());
  if (!current || *current != report.source())
    return Result<std::size_t>::failure(invalid("STEP source intent changed since inspection"));
  if (!report.observed_sha256_digest() || (report.state() != StepImportFreshness::Available &&
                                           report.state() != StepImportFreshness::Changed))
    return Result<std::size_t>::failure(
        invalid("STEP source refresh requires a readable inspected source"));
  auto checked = inspect_step_import_source(report.source(), root);
  if (!checked)
    return Result<std::size_t>::failure(checked.error());
  if (checked.value().root_directory() != report.root_directory() ||
      checked.value().observed_sha256_digest() != report.observed_sha256_digest() ||
      (checked.value().state() != StepImportFreshness::Available &&
       checked.value().state() != StepImportFreshness::Changed))
    return Result<std::size_t>::failure(
        invalid("STEP source changed or became unavailable since inspection; inspect again"));
  return host.refresh_step_import_source(report.source(),
                                         *checked.value().observed_sha256_digest());
}
} // namespace
std::string_view to_string(StepImportFreshness state) noexcept {
  switch (state) {
  case StepImportFreshness::Available:
    return "available";
  case StepImportFreshness::Missing:
    return "missing";
  case StepImportFreshness::Changed:
    return "changed";
  case StepImportFreshness::Unreadable:
    return "unreadable";
  }
  return "unknown";
}
Result<std::string> step_import_sha256(std::string_view bytes) {
  auto context = digest_context();
  if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1 ||
      EVP_DigestUpdate(context.get(), bytes.data(), bytes.size()) != 1)
    return Result<std::string>::failure(crypto_error());
  return finish_digest(context.get());
}
Result<StepImportAssetInspection>
inspect_step_import_asset(const std::filesystem::path& root_directory,
                          std::string_view relative_path) {
  auto asset = read_asset(root_directory, relative_path, false);
  if (!asset)
    return Result<StepImportAssetInspection>::failure(asset.error());
  auto& read = asset.value();
  return Result<StepImportAssetInspection>::success(
      StepImportAssetInspection(std::move(read.root), std::move(read.path), read.state,
                                std::move(read.digest), std::move(read.diagnostic)));
}
Result<StepImportFreshnessReport> inspect_step_import_source(const StepImportSource& source,
                                                             const std::filesystem::path& root) {
  auto asset = inspect_step_import_asset(root, source.project_relative_asset_path());
  if (!asset)
    return Result<StepImportFreshnessReport>::failure(asset.error());
  return Result<StepImportFreshnessReport>::success(
      StepImportFreshnessReport(source, std::move(asset.value())));
}
Result<std::string> read_verified_step_import_source(const StepImportSource& source,
                                                     const std::filesystem::path& root) {
  auto asset = read_asset(root, source.project_relative_asset_path(), true);
  if (!asset)
    return Result<std::string>::failure(asset.error());
  if (asset.value().state != StepImportFreshness::Available)
    return Result<std::string>::failure(invalid(std::move(asset.value().diagnostic)));
  if (!asset.value().digest || *asset.value().digest != source.sha256_digest())
    return Result<std::string>::failure(
        invalid("STEP source content differs from its registered SHA-256 digest"));
  return Result<std::string>::success(std::move(asset.value().bytes));
}
Result<std::size_t> refresh_step_import_source(PartDocument& document,
                                               const std::filesystem::path& root,
                                               const StepImportFreshnessReport& report) {
  return refresh(document, root, report);
}
Result<std::size_t> refresh_step_import_source(Project& project, const std::filesystem::path& root,
                                               const StepImportFreshnessReport& report) {
  return refresh(project, root, report);
}
} // namespace blcad
