#include "blcad/core/step_import_source.hpp"
#include <algorithm>
#include <cctype>
#include <utility>

namespace blcad {
namespace {
Error invalid(std::string text) {
  return Error::validation("step_import_source", std::move(text));
}
bool control(unsigned char c) {
  return c < 32 || c == 127;
}
bool blank(std::string_view text) {
  return text.empty() ||
         std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c); });
}
bool reserved(std::string name) {
  name = name.substr(0, name.find('.'));
  std::transform(name.begin(), name.end(), name.begin(),
                 [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return name == "CON" || name == "PRN" || name == "AUX" || name == "NUL" ||
         (name.size() == 4 && (name.starts_with("COM") || name.starts_with("LPT")) &&
          name[3] >= '1' && name[3] <= '9');
}
} // namespace
std::string_view to_string(StepImportMode mode) noexcept {
  switch (mode) {
  case StepImportMode::Reference:
    return "reference";
  case StepImportMode::EditableBody:
    return "editable_body";
  }
  return "unknown";
}
Result<std::string> canonical_step_import_asset_path(std::string_view path) {
  if (path.empty() || path.front() == '/' || path.back() == '/' ||
      std::any_of(path.begin(), path.end(), [](unsigned char c) {
        return control(c) || c == '\\' || c == ':' || c == '<' || c == '>' || c == '"' ||
               c == '|' || c == '?' || c == '*';
      }))
    return Result<std::string>::failure(
        invalid("STEP source requires a portable relative asset file path"));
  std::string canonical;
  std::size_t offset = 0;
  while (offset < path.size()) {
    const auto end = path.find('/', offset);
    const auto component =
        path.substr(offset, end == std::string_view::npos ? path.size() - offset : end - offset);
    if (component == "..")
      return Result<std::string>::failure(
          invalid("STEP source paths must not contain parent traversal"));
    if (!component.empty() && component != ".") {
      if (component.back() == '.' || component.back() == ' ' || reserved(std::string(component)))
        return Result<std::string>::failure(
            invalid("STEP source path contains a non-portable filename"));
      if (!canonical.empty())
        canonical += '/';
      canonical += component;
    }
    if (end == std::string_view::npos)
      break;
    offset = end + 1;
  }
  if (canonical.empty() || path.ends_with("/."))
    return Result<std::string>::failure(invalid("STEP source path must identify an asset file"));
  return Result<std::string>::success(std::move(canonical));
}
StepImportSource::StepImportSource(StepImportSourceId id, std::string name, std::string path,
                                   std::string digest, StepImportMode mode)
    : id_(std::move(id)), name_(std::move(name)), path_(std::move(path)),
      digest_(std::move(digest)), mode_(mode) {}
Result<StepImportSource> StepImportSource::create(StepImportSourceId id, std::string name,
                                                  std::string path, std::string digest,
                                                  StepImportMode mode) {
  if (blank(id.value()) || blank(name) ||
      std::any_of(id.value().begin(), id.value().end(), control) ||
      std::any_of(name.begin(), name.end(), control))
    return Result<StepImportSource>::failure(
        invalid("STEP source id and name must be nonempty text without controls"));
  if (mode != StepImportMode::Reference && mode != StepImportMode::EditableBody)
    return Result<StepImportSource>::failure(
        invalid("STEP import mode must be explicit Reference or EditableBody"));
  auto normalized = canonical_step_import_asset_path(path);
  if (!normalized)
    return Result<StepImportSource>::failure(normalized.error());
  if (digest.size() != 64 || !std::all_of(digest.begin(), digest.end(), [](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
      }))
    return Result<StepImportSource>::failure(
        invalid("STEP SHA-256 digest must contain exactly 64 hexadecimal characters"));
  std::transform(digest.begin(), digest.end(), digest.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return Result<StepImportSource>::success(StepImportSource(
      std::move(id), std::move(name), std::move(normalized.value()), std::move(digest), mode));
}
Result<StepImportSource> StepImportSource::with_sha256_digest(std::string digest) const {
  return create(id_, name_, path_, std::move(digest), mode_);
}
const StepImportSource* StepImportSourceRegistry::find(StepImportSourceId id) const noexcept {
  const auto found = std::find_if(sources_.begin(), sources_.end(),
                                  [&](const auto& source) { return source.id() == id; });
  return found == sources_.end() ? nullptr : &*found;
}
Result<std::size_t> StepImportSourceRegistry::add(StepImportSource source) {
  if (find(source.id()))
    return Result<std::size_t>::failure(invalid("STEP source id already exists"));
  if (std::any_of(sources_.begin(), sources_.end(), [&](const auto& other) {
        return other.project_relative_asset_path() == source.project_relative_asset_path();
      }))
    return Result<std::size_t>::failure(invalid("STEP source canonical asset path already exists"));
  sources_.push_back(std::move(source));
  return Result<std::size_t>::success(1);
}
Result<std::size_t> StepImportSourceRegistry::remove(StepImportSourceId id) {
  const auto found = std::find_if(sources_.begin(), sources_.end(),
                                  [&](const auto& source) { return source.id() == id; });
  if (found == sources_.end())
    return Result<std::size_t>::failure(invalid("STEP source does not exist"));
  sources_.erase(found);
  return Result<std::size_t>::success(1);
}
Result<std::size_t> StepImportSourceRegistry::remove(const StepImportSource& expected) {
  const auto* current = find(expected.id());
  if (!current || *current != expected)
    return Result<std::size_t>::failure(invalid("STEP source intent changed since inspection"));
  return remove(expected.id());
}
Result<std::size_t> StepImportSourceRegistry::refresh(const StepImportSource& expected,
                                                      std::string digest) {
  auto found = std::find_if(sources_.begin(), sources_.end(),
                            [&](const auto& source) { return source.id() == expected.id(); });
  if (found == sources_.end() || *found != expected)
    return Result<std::size_t>::failure(invalid("STEP source intent changed since inspection"));
  auto replacement = expected.with_sha256_digest(std::move(digest));
  if (!replacement)
    return Result<std::size_t>::failure(replacement.error());
  if (*found == replacement.value())
    return Result<std::size_t>::success(0);
  *found = std::move(replacement.value());
  return Result<std::size_t>::success(1);
}
} // namespace blcad
