#include "blcad/core/body.hpp"

#include <utility>

namespace blcad {

std::string_view to_string(BodyKind kind) noexcept {
  switch (kind) {
  case BodyKind::Solid:
    return "solid";
  case BodyKind::Surface:
    return "surface";
  }
  return "solid";
}

std::string_view to_string(BodyVisibility visibility) noexcept {
  switch (visibility) {
  case BodyVisibility::Visible:
    return "visible";
  case BodyVisibility::Hidden:
    return "hidden";
  }
  return "visible";
}

Result<Body> Body::create(BodyId id, std::string name, BodyKind kind, BodyVisibility visibility) {
  const std::string object_id = id.empty() ? "body" : id.value();
  if (id.empty()) {
    return Result<Body>::failure(Error::validation(object_id, "body id must not be empty"));
  }
  if (name.empty()) {
    return Result<Body>::failure(Error::validation(object_id, "body name must not be empty"));
  }
  return Result<Body>::success(Body(std::move(id), std::move(name), kind, visibility));
}

Result<Body> Body::with_visibility(BodyVisibility visibility) const {
  return create(id_, name_, kind_, visibility);
}

Body::Body(BodyId id, std::string name, BodyKind kind, BodyVisibility visibility)
    : id_(std::move(id)), name_(std::move(name)), kind_(kind), visibility_(visibility) {}

const BodyId& Body::id() const noexcept {
  return id_;
}

const std::string& Body::name() const noexcept {
  return name_;
}

BodyKind Body::kind() const noexcept {
  return kind_;
}

BodyVisibility Body::visibility() const noexcept {
  return visibility_;
}

} // namespace blcad
