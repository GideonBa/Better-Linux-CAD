#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"

#include <string>
#include <string_view>

namespace blcad {

enum class BodyKind { Solid, Surface };
[[nodiscard]] std::string_view to_string(BodyKind kind) noexcept;

enum class BodyVisibility { Visible, Hidden };
[[nodiscard]] std::string_view to_string(BodyVisibility visibility) noexcept;

class Body {
public:
  [[nodiscard]] static Result<Body> create(BodyId id, std::string name, BodyKind kind,
                                           BodyVisibility visibility = BodyVisibility::Visible);

  [[nodiscard]] Result<Body> with_visibility(BodyVisibility visibility) const;

  [[nodiscard]] const BodyId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] BodyKind kind() const noexcept;
  [[nodiscard]] BodyVisibility visibility() const noexcept;

  friend bool operator==(const Body&, const Body&) = default;

private:
  Body(BodyId id, std::string name, BodyKind kind, BodyVisibility visibility);

  BodyId id_;
  std::string name_;
  BodyKind kind_ = BodyKind::Solid;
  BodyVisibility visibility_ = BodyVisibility::Visible;
};

} // namespace blcad
