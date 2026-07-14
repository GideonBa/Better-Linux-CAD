#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace blcad {

enum class SketchCoordinate3DSource { Explicit, Parameter };

class SketchCoordinate3D {
public:
  [[nodiscard]] static Result<SketchCoordinate3D> create_explicit(Quantity coordinate);
  [[nodiscard]] static Result<SketchCoordinate3D> create_parameter(ParameterId parameter);

  [[nodiscard]] SketchCoordinate3DSource source() const noexcept;
  [[nodiscard]] const std::optional<Quantity>& explicit_coordinate() const noexcept;
  [[nodiscard]] const std::optional<ParameterId>& parameter() const noexcept;

private:
  SketchCoordinate3D(SketchCoordinate3DSource source, std::optional<Quantity> explicit_coordinate,
                     std::optional<ParameterId> parameter);

  SketchCoordinate3DSource source_;
  std::optional<Quantity> explicit_coordinate_;
  std::optional<ParameterId> parameter_;
};

class SketchPoint3D {
public:
  [[nodiscard]] static Result<SketchPoint3D> create(SketchEntityId id, SketchCoordinate3D x,
                                                    SketchCoordinate3D y, SketchCoordinate3D z);

  [[nodiscard]] const SketchEntityId& id() const noexcept;
  [[nodiscard]] const SketchCoordinate3D& x() const noexcept;
  [[nodiscard]] const SketchCoordinate3D& y() const noexcept;
  [[nodiscard]] const SketchCoordinate3D& z() const noexcept;
  [[nodiscard]] std::vector<ParameterId> parameter_dependencies() const;

private:
  SketchPoint3D(SketchEntityId id, SketchCoordinate3D x, SketchCoordinate3D y,
                SketchCoordinate3D z);

  SketchEntityId id_;
  SketchCoordinate3D x_;
  SketchCoordinate3D y_;
  SketchCoordinate3D z_;
};

class SketchLine3D {
public:
  [[nodiscard]] static Result<SketchLine3D> create(SketchEntityId id, SketchEntityId start_point,
                                                   SketchEntityId end_point);

  [[nodiscard]] const SketchEntityId& id() const noexcept;
  [[nodiscard]] const SketchEntityId& start_point() const noexcept;
  [[nodiscard]] const SketchEntityId& end_point() const noexcept;

private:
  SketchLine3D(SketchEntityId id, SketchEntityId start_point, SketchEntityId end_point);

  SketchEntityId id_;
  SketchEntityId start_point_;
  SketchEntityId end_point_;
};

class SketchPolyline3D {
public:
  [[nodiscard]] static Result<SketchPolyline3D>
  create(SketchEntityId id, std::vector<SketchEntityId> ordered_vertices);

  [[nodiscard]] const SketchEntityId& id() const noexcept;
  [[nodiscard]] const std::vector<SketchEntityId>& ordered_vertices() const noexcept;

private:
  SketchPolyline3D(SketchEntityId id, std::vector<SketchEntityId> ordered_vertices);

  SketchEntityId id_;
  std::vector<SketchEntityId> ordered_vertices_;
};

class Sketch3D {
public:
  [[nodiscard]] static Result<Sketch3D> create(Sketch3DId id, std::string name);

  [[nodiscard]] Result<std::size_t> add_point(SketchPoint3D point);
  [[nodiscard]] Result<std::size_t> add_line(SketchLine3D line);
  [[nodiscard]] Result<std::size_t> add_polyline(SketchPolyline3D polyline);
  [[nodiscard]] Result<std::size_t> remove_point(SketchEntityId id);
  [[nodiscard]] Result<std::size_t> remove_line(SketchEntityId id);
  [[nodiscard]] Result<std::size_t> remove_polyline(SketchEntityId id);

  [[nodiscard]] const Sketch3DId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const std::vector<SketchPoint3D>& points() const noexcept;
  [[nodiscard]] const std::vector<SketchLine3D>& lines() const noexcept;
  [[nodiscard]] const std::vector<SketchPolyline3D>& polylines() const noexcept;
  [[nodiscard]] const SketchPoint3D* find_point(SketchEntityId id) const noexcept;
  [[nodiscard]] const SketchLine3D* find_line(SketchEntityId id) const noexcept;
  [[nodiscard]] const SketchPolyline3D* find_polyline(SketchEntityId id) const noexcept;
  [[nodiscard]] std::vector<ParameterId> parameter_dependencies() const;
  [[nodiscard]] std::size_t entity_count() const noexcept;

private:
  Sketch3D(Sketch3DId id, std::string name);
  [[nodiscard]] bool has_entity_id(const SketchEntityId& id) const noexcept;

  Sketch3DId id_;
  std::string name_;
  std::vector<SketchPoint3D> points_;
  std::vector<SketchLine3D> lines_;
  std::vector<SketchPolyline3D> polylines_;
};

} // namespace blcad
