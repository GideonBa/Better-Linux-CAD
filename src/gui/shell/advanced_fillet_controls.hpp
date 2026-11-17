#pragma once
#include "blcad/core/advanced_fillet_intent.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/gui/gui_fillet_draft.hpp"
#include "blcad/gui/gui_interactive_finishing.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <memory>
namespace blcad::gui {
// UI-only staged data. Core factories and the shared transaction own validation/publication.
struct AdvancedFilletControls {
  QWidget* panel;
  QComboBox *mode, *group, *interpolation, *propagation, *continuity, *size_binding;
  QLineEdit *group_name, *size_value, *setback_value;
  QListWidget* members;
  QTableWidget* stations;
  QCheckBox *corner, *independent;
  QLabel *capability, *derived, *size_label;
  std::array<QComboBox*, 2> hold;
  QWidget *group_buttons, *station_buttons, *size_row, *corner_row, *law_row, *contact_row;
  const PartDocument* document{};
  FeatureId id{""};
  BodyId body{""};
  std::string name;
  std::optional<PartDocument> input_document;
  std::optional<geometry::ShapeCache> input_cache;
  std::optional<std::size_t> input_prefix;
  GuiSelectionModel entry_selection;
  std::optional<GuiSelection> entry_viewport_selection;
  bool editing = false, updating = false, failure = false;
  std::size_t sequence = 0;
  std::vector<EdgeReference> constant_edges;
  std::vector<FilletEdgeGroup> groups;
  FilletFaceSet first{"first", "First side", {}}, second{"second", "Second side", {}},
      middle{"middle", "Middle", {}};
  std::vector<FilletHoldLine> holds;
  std::optional<FilletSetbackCorner> policy;
  ParameterId size{""};
  std::vector<Parameter> pending;
  std::map<std::string, double> values;
  std::map<std::string, QString> labels;
  std::shared_ptr<GuiFilletDraft> live;
  std::function<void()> changed, scope_changed;
  explicit AdvancedFilletControls(QWidget* parent, QComboBox* shared_mode,
                                  std::function<void()> change, std::function<void()> scope)
      : mode(shared_mode), changed(std::move(change)), scope_changed(std::move(scope)) {
    panel = new QWidget(parent);
    auto* column = new QVBoxLayout(panel);
    column->setContentsMargins(0, 0, 0, 0);
    const auto combo = [&](const char* key, QStringList options) {
      auto* c = new QComboBox(panel);
      c->setObjectName(QString("blcad.fillet.") + key);
      c->addItems(options);
      c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
      return c;
    };
    const auto row = [&](const char* title, QWidget* widget) {
      auto* p = new QWidget(panel);
      auto* layout = new QFormLayout(p);
      layout->setContentsMargins(0, 0, 0, 0);
      layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
      layout->addRow(title, widget);
      column->addWidget(p);
      return p;
    };
    group = combo("group", {});
    row("Input Group / Role", group);
    group_name = new QLineEdit(panel);
    group_name->setObjectName("blcad.fillet.group_name");
    row("Name", group_name);
    const auto buttons =
        [&](std::initializer_list<std::pair<const char*, std::function<void()>>> actions) {
          auto* p = new QWidget(panel);
          auto* layout = new QHBoxLayout(p);
          layout->setContentsMargins(0, 0, 0, 0);
          for (const auto& [text, action] : actions) {
            auto* b = new QPushButton(text, p);
            layout->addWidget(b);
            QObject::connect(b, &QPushButton::clicked, p, [this, action] {
              if (!updating)
                action();
            });
          }
          column->addWidget(p);
          return p;
        };
    group_buttons = buttons({{"Add Group",
                              [this] {
                                add_group();
                                configure();
                                group->setCurrentIndex(int(groups.size()) - 1);
                                scope_changed();
                              }},
                             {"Remove Group",
                              [this] {
                                const int i = group->currentIndex();
                                if (i >= 0 && i < int(groups.size()))
                                  groups.erase(groups.begin() + i);
                                configure();
                                scope_changed();
                              }},
                             {"Reverse Chain", [this] {
                                if (auto* g = active_group()) {
                                  *g = reverse_fillet_group(*g);
                                  configure();
                                  scope_changed();
                                }
                              }}});
    members = new QListWidget(panel);
    members->setObjectName("blcad.fillet.members");
    members->setMaximumHeight(90);
    column->addWidget(members);
    buttons({{"Move Up", [this] { move_member(-1); }},
             {"Move Down", [this] { move_member(1); }},
             {"Remove Input", [this] { remove_member(); }}});
    stations = new QTableWidget(0, 4, panel);
    stations->setObjectName("blcad.fillet.stations");
    stations->setHorizontalHeaderLabels({"Station", "Fraction", "Binding", "Radius (mm)"});
    stations->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    stations->verticalHeader()->hide();
    stations->setMinimumHeight(132);
    column->addWidget(stations);
    station_buttons = buttons(
        {{"Add Station",
          [this] {
            if (auto* g = active_group()) {
              const int i = std::clamp(stations->currentRow() + 1, 1, int(g->stations.size()) - 1);
              const double fraction =
                  (g->stations[i - 1].normalized_length() + g->stations[i].normalized_length()) / 2;
              const auto radius = new_radius(1);
              g->stations.insert(
                  g->stations.begin() + i,
                  {new_id("station"), FilletStationLocation::Fraction, fraction, radius});
              render_stations();
              changed();
            }
          }},
         {"Remove Station", [this] {
            if (auto* g = active_group()) {
              const int i = stations->currentRow();
              if (i > 0 && i + 1 < int(g->stations.size())) {
                g->stations.erase(g->stations.begin() + i);
                render_stations();
                changed();
              }
            }
          }}});
    interpolation = combo("interpolation", {"Linear", "Shape-preserving Cubic"});
    propagation = combo("propagation", {"Explicit Complete Chain", "Expand Tangent Chain"});
    auto* law = new QWidget(panel);
    auto* form = new QFormLayout(law);
    form->setContentsMargins(0, 0, 0, 0);
    form->addRow("Interpolation", interpolation);
    form->addRow("Propagation", propagation);
    column->addWidget(law);
    law_row = law;
    continuity = combo("continuity", {"Tangent (G1)", "Curvature (G2)"});
    contact_row = row("Contact", continuity);
    size_binding = combo("size_binding", {});
    size_value = new QLineEdit(panel);
    size_value->setObjectName("blcad.fillet.size");
    size_row = new QWidget(panel);
    auto* sizes = new QFormLayout(size_row);
    sizes->setContentsMargins(0, 0, 0, 0);
    size_label = new QLabel("Radius", size_row);
    sizes->addRow("Binding", size_binding);
    sizes->addRow(size_label, size_value);
    column->addWidget(size_row);
    derived = new QLabel("Derived from selected side supports", panel);
    derived->setObjectName("blcad.fillet.derived_size");
    derived->setWordWrap(true);
    column->addWidget(derived);
    for (int i = 0; i < 2; ++i) {
      hold[i] = combo(i == 0 ? "hold_first" : "hold_second", {});
      row(i == 0 ? "First Hold Line" : "Second Hold Line", hold[i]);
      QObject::connect(hold[i], &QComboBox::currentIndexChanged, panel, [this, i] {
        if (updating)
          return;
        const auto side = i == 0 ? FilletSide::First : FilletSide::Second;
        const auto path = hold[i]->currentData().toString().toStdString();
        auto it =
            std::find_if(holds.begin(), holds.end(), [&](const auto& h) { return h.side == side; });
        if (path.empty()) {
          if (it != holds.end())
            holds.erase(it);
        } else if (it != holds.end())
          it->path = PathCurveId(path);
        else
          holds.push_back({new_id("hold"), side, PathCurveId(path)});
        changed();
      });
    }
    corner = new QCheckBox("Setback at shared corner", panel);
    corner->setObjectName("blcad.fillet.setback");
    setback_value = new QLineEdit(panel);
    setback_value->setObjectName("blcad.fillet.setback_size");
    corner_row = new QWidget(panel);
    auto* corners = new QFormLayout(corner_row);
    corners->setContentsMargins(0, 0, 0, 0);
    corners->addRow(corner);
    corners->addRow("Setback (mm)", setback_value);
    column->addWidget(corner_row);
    independent = new QCheckBox("Make changed shared values independent", panel);
    independent->setObjectName("blcad.fillet.independent_values");
    column->addWidget(independent);
    capability = new QLabel(panel);
    capability->setObjectName("blcad.fillet.capability");
    capability->setWordWrap(true);
    column->addWidget(capability);
    QObject::connect(group, &QComboBox::currentIndexChanged, panel, [this] {
      if (!updating) {
        configure();
        scope_changed();
      }
    });
    QObject::connect(group_name, &QLineEdit::editingFinished, panel, [this] {
      if (updating)
        return;
      if (auto* g = active_group())
        g->name = group_name->text().toStdString();
      else if (auto* f = active_faces())
        f->name = group_name->text().toStdString();
      configure();
      changed();
    });
    QObject::connect(interpolation, &QComboBox::currentIndexChanged, panel, [this](int i) {
      if (!updating)
        if (auto* g = active_group()) {
          g->interpolation =
              i == 0 ? FilletRadiusInterpolation::Linear : FilletRadiusInterpolation::MonotoneCubic;
          changed();
        }
    });
    QObject::connect(propagation, &QComboBox::currentIndexChanged, panel, [this](int i) {
      if (!updating)
        if (auto* g = active_group()) {
          g->propagation = i == 0 ? FilletPropagation::Explicit : FilletPropagation::TangentChain;
          changed();
        }
    });
    QObject::connect(continuity, &QComboBox::currentIndexChanged, panel, [this] {
      if (!updating) {
        configure();
        changed();
      }
    });
    QObject::connect(size_binding, &QComboBox::currentIndexChanged, panel, [this] {
      if (!updating) {
        size = ParameterId(size_binding->currentData().toString().toStdString());
        size_value->setText(number(value(size)));
        changed();
      }
    });
    QObject::connect(size_value, &QLineEdit::editingFinished, panel, [this] {
      if (!updating)
        set_value(size, size_value);
    });
    QObject::connect(corner, &QCheckBox::toggled, panel, [this](bool enabled) {
      if (updating)
        return;
      if (enabled && groups.size() == 3) {
        const auto parameter = policy ? policy->distance : new_radius(3);
        policy = FilletSetbackCorner{policy ? policy->id : new_id("corner"),
                                     {groups[0].id, groups[1].id, groups[2].id},
                                     parameter};
        setback_value->setText(number(value(parameter)));
      }
      changed();
    });
    QObject::connect(setback_value, &QLineEdit::editingFinished, panel, [this] {
      if (!updating && policy)
        set_value(policy->distance, setback_value);
    });
    QObject::connect(independent, &QCheckBox::toggled, panel, [this] {
      if (!updating)
        changed();
    });
  }
  static QString number(double v) {
    return QString::number(v, 'g', 17);
  }
  int current_mode() const {
    return mode->currentIndex();
  }
  FilletEdgeGroup* active_group() {
    const int i = group->currentIndex();
    return current_mode() == 1 && i >= 0 && i < int(groups.size()) ? &groups[i] : nullptr;
  }
  FilletFaceSet* active_faces() {
    if (current_mode() < 2)
      return nullptr;
    return group->currentIndex() == 0 ? &first : group->currentIndex() == 1 ? &second : &middle;
  }
  PartFeatureInputRole role() const {
    return current_mode() < 2 ? PartFeatureInputRole::FilletEdge
           : current_mode() == 3 && group->currentIndex() == 2
               ? PartFeatureInputRole::FilletMiddleFace
               : PartFeatureInputRole::FilletSupportFace;
  }
  std::string new_id(const char* kind) {
    for (;;) {
      auto candidate = id.value() + "." + kind + "." + std::to_string(++sequence);
      bool exists = false;
      for (const auto& g : groups) {
        exists |= g.id == candidate;
        for (const auto& station : g.stations)
          exists |= station.id == candidate;
      }
      for (const auto& h : holds)
        exists |= h.id == candidate;
      if (policy)
        exists |= policy->id == candidate;
      if (!exists)
        return candidate;
    }
  }
  ParameterId new_radius(double v) {
    ParameterId parameter("");
    std::string label;
    do {
      parameter = ParameterId(new_id("size"));
      label = name + " Size " + std::to_string(sequence);
    } while (document && (document->find_parameter(parameter) ||
                          std::any_of(document->parameters().begin(), document->parameters().end(),
                                      [&](const auto& p) { return p.name() == label; })));
    auto quantity = Quantity::length_mm(v);
    auto p = quantity ? Parameter::create_length(parameter, label, quantity.value())
                      : Result<Parameter>::failure(quantity.error());
    if (p)
      pending.push_back(p.value());
    return parameter;
  }
  double value(const ParameterId& parameter) const {
    if (live) {
      auto it = live->values.find(parameter.value());
      if (it != live->values.end())
        return it->second;
    }
    if (auto it = values.find(parameter.value()); it != values.end())
      return it->second;
    for (const auto& p : pending)
      if (p.id() == parameter)
        return p.value().millimeters();
    if (document)
      if (auto p = document->find_parameter(parameter))
        return p->value().millimeters();
    return 0;
  }
  void set_value(const ParameterId& parameter, QLineEdit* input) {
    bool ok = false;
    double v = input->text().toDouble(&ok);
    if (!ok || !std::isfinite(v) || v <= 0) {
      capability->setText("Enter a positive finite size in millimeters.");
      return;
    }
    if (v != value(parameter)) {
      values[parameter.value()] = v;
      if (live)
        live->values[parameter.value()] = v;
      changed();
    }
  }
  void fill_bindings(QComboBox* box, const ParameterId& selected) {
    QSignalBlocker block(box);
    box->clear();
    if (document)
      for (const auto& p : document->parameters())
        if (p.type() == ParameterType::Length)
          box->addItem(QString::fromStdString(p.name()), QString::fromStdString(p.id().value()));
    for (const auto& p : pending)
      box->addItem(QString::fromStdString(p.name()), QString::fromStdString(p.id().value()));
    int index = box->findData(QString::fromStdString(selected.value()));
    if (index < 0) {
      box->addItem("Saved binding (repair)", QString::fromStdString(selected.value()));
      index = box->count() - 1;
    }
    box->setCurrentIndex(index);
  }
  void add_group() {
    auto start = new_radius(1), end = new_radius(2);
    groups.push_back({new_id("group"),
                      "Edge Group " + std::to_string(groups.size() + 1),
                      {},
                      FilletPropagation::Explicit,
                      FilletRadiusInterpolation::Linear,
                      {{new_id("station"), FilletStationLocation::Start, 0, start},
                       {new_id("station"), FilletStationLocation::End, 1, end}}});
  }
  void reset(const PartDocument& source, FeatureId feature, std::string title, BodyId target) {
    QScopedValueRollback<bool> guard(updating, true);
    document = &source;
    id = feature;
    name = std::move(title);
    body = target;
    editing = false;
    input_document.reset();
    input_cache.reset();
    input_prefix.reset();
    sequence = 0;
    constant_edges.clear();
    groups.clear();
    first = {"first", "First side", {}};
    second = {"second", "Second side", {}};
    middle = {"middle", "Middle", {}};
    pending.clear();
    values.clear();
    labels.clear();
    holds.clear();
    policy.reset();
    live.reset();
    size = ParameterId("");
    corner->setChecked(false);
    independent->setChecked(false);
    continuity->setCurrentIndex(0);
    group->clear();
    configure();
  }
  void load(const PartDocument& source, const FilletFeature& record) {
    reset(source, record.id(), record.name(), record.target_body());
    QScopedValueRollback<bool> guard(updating, true);
    editing = true;
    if (!record.advanced_intent()) {
      mode->setCurrentIndex(0);
      constant_edges = record.edges();
      size = record.radius_parameter();
    } else
      std::visit(
          [&](const auto& intent) {
            using T = std::decay_t<decltype(intent)>;
            if constexpr (std::is_same_v<T, VariableFilletIntent>) {
              mode->setCurrentIndex(1);
              groups = intent.groups;
              policy = intent.setback;
              corner->setChecked(bool(policy));
            } else {
              first = intent.first;
              second = intent.second;
              holds = intent.hold_lines;
              continuity->setCurrentIndex(intent.continuity == FilletContinuity::Curvature ? 1 : 0);
              if constexpr (std::is_same_v<T, FaceBlendIntent>) {
                mode->setCurrentIndex(2);
                size = intent.size;
              } else {
                mode->setCurrentIndex(3);
                middle = intent.middle;
              }
            }
          },
          record.advanced_intent()->definition);
    configure();
  }
  void configure() {
    QScopedValueRollback<bool> guard(updating, true);
    const int m = current_mode(), selected = std::max(0, group->currentIndex());
    if (m == 1 && groups.empty())
      add_group();
    if ((m == 0 || m == 2) && size.empty())
      size = new_radius(1);
    group->clear();
    if (m == 1)
      for (const auto& g : groups)
        group->addItem(QString::fromStdString(g.name));
    else if (m >= 2) {
      group->addItems({"First Side", "Second Side"});
      if (m == 3)
        group->addItem("Middle Faces");
    }
    group->setCurrentIndex(std::min(selected, group->count() - 1));
    group->parentWidget()->setVisible(m != 0);
    group_name->parentWidget()->setVisible(m != 0);
    group_buttons->setVisible(m == 1);
    members->setVisible(true);
    stations->setVisible(m == 1);
    station_buttons->setVisible(m == 1);
    law_row->setVisible(m == 1);
    contact_row->setVisible(m >= 2);
    size_row->setVisible(m == 0 || m == 2);
    derived->setVisible(m == 3);
    corner_row->setVisible(m == 1);
    for (auto* h : hold)
      h->parentWidget()->setVisible(m >= 2);
    if (auto* g = active_group()) {
      group_name->setText(QString::fromStdString(g->name));
      interpolation->setCurrentIndex(
          g->interpolation == FilletRadiusInterpolation::MonotoneCubic ? 1 : 0);
      propagation->setCurrentIndex(g->propagation == FilletPropagation::TangentChain ? 1 : 0);
    } else if (auto* f = active_faces())
      group_name->setText(QString::fromStdString(f->name));
    if (m == 0 || m == 2) {
      fill_bindings(size_binding, size);
      size_value->setText(number(value(size)));
      size_label->setText(m == 2 && continuity->currentIndex() == 1 ? "Contact Setback (mm)"
                                                                    : "Radius (mm)");
    }
    for (int i = 0; i < 2; ++i) {
      hold[i]->clear();
      hold[i]->addItem("None", "");
      if (document)
        for (const auto& path : document->path_curves())
          hold[i]->addItem(QString::fromStdString(path.name()),
                           QString::fromStdString(path.id().value()));
      for (const auto& h : holds)
        if ((h.side == FilletSide::First ? 0 : 1) == i) {
          int found = hold[i]->findData(QString::fromStdString(h.path.value()));
          if (found < 0) {
            hold[i]->addItem("Saved hold line (repair)", QString::fromStdString(h.path.value()));
            found = hold[i]->count() - 1;
          }
          hold[i]->setCurrentIndex(found);
        }
    }
    if (policy)
      setback_value->setText(number(value(policy->distance)));
    capability->setText(
        m == 1
            ? "Stations use true contour length. Setback requires three explicit orthogonal "
              "single-edge groups with one constant shared radius and setback greater than radius."
        : m == 2 ? "G1 uses a radius. G2 uses contact setback between orthogonal convex planes. "
                   "Hold lines constrain the complete contact."
        : m == 3 ? "Select opposed planar or coaxial cylindrical sides and the middle to replace. "
                   "Size is derived from the sides."
                 : "Select an ordered chain of sharp edges.");
    render_members();
    render_stations();
  }
  void render_members() {
    QSignalBlocker block(members);
    members->clear();
    const auto append = [&](const auto& reference) {
      std::string key;
      if constexpr (std::is_same_v<std::decay_t<decltype(reference)>, EdgeReference>)
        key = interactive_edge_identity(reference);
      else
        key = interactive_face_identity(reference);
      auto found = labels.find(key);
      members->addItem(found == labels.end() ? "Saved input (select to repair)" : found->second);
    };
    if (auto* g = active_group())
      for (const auto& e : g->edges)
        append(e.edge);
    else if (auto* f = active_faces())
      for (const auto& r : f->faces)
        append(r);
    else
      for (const auto& e : constant_edges)
        append(e);
  }
  void render_stations() {
    QScopedValueRollback<bool> guard(updating, true);
    stations->setRowCount(0);
    auto* g = active_group();
    if (!g)
      return;
    stations->setRowCount(int(g->stations.size()));
    const auto group_id = g->id;
    for (int row = 0; row < int(g->stations.size()); ++row) {
      const auto saved = g->stations[row];
      auto* location = new QComboBox(stations);
      location->addItems({"At Length", "Start", "End"});
      location->setCurrentIndex(saved.location == FilletStationLocation::Start ? 1
                                : saved.location == FilletStationLocation::End ? 2
                                                                               : 0);
      stations->setCellWidget(row, 0, location);
      auto* fraction = new QLineEdit(number(saved.fraction), stations);
      fraction->setEnabled(saved.location == FilletStationLocation::Fraction);
      stations->setCellWidget(row, 1, fraction);
      auto* binding = new QComboBox(stations);
      fill_bindings(binding, saved.radius);
      stations->setCellWidget(row, 2, binding);
      auto* radius = new QLineEdit(number(value(saved.radius)), stations);
      stations->setCellWidget(row, 3, radius);
      const auto station = [this, group_id, id = saved.id]() -> FilletRadiusStation* {
        for (auto& group : groups)
          if (group.id == group_id)
            for (auto& s : group.stations)
              if (s.id == id)
                return &s;
        return nullptr;
      };
      QObject::connect(location, &QComboBox::currentIndexChanged, panel,
                       [this, station, fraction](int index) {
                         if (updating)
                           return;
                         if (auto* s = station()) {
                           s->location = index == 1   ? FilletStationLocation::Start
                                         : index == 2 ? FilletStationLocation::End
                                                      : FilletStationLocation::Fraction;
                           fraction->setEnabled(index == 0);
                           changed();
                         }
                       });
      QObject::connect(fraction, &QLineEdit::editingFinished, panel, [this, station, fraction] {
        if (updating)
          return;
        bool ok = false;
        double v = fraction->text().toDouble(&ok);
        if (ok && std::isfinite(v) && v >= 0 && v <= 1)
          if (auto* s = station()) {
            if (s->fraction != v) {
              s->fraction = v;
              changed();
            }
          }
      });
      QObject::connect(binding, &QComboBox::currentIndexChanged, panel,
                       [this, station, binding, radius] {
                         if (updating)
                           return;
                         if (auto* s = station()) {
                           s->radius = ParameterId(binding->currentData().toString().toStdString());
                           radius->setText(number(value(s->radius)));
                           changed();
                         }
                       });
      QObject::connect(radius, &QLineEdit::editingFinished, panel, [this, station, radius] {
        if (!updating)
          if (auto* s = station())
            set_value(s->radius, radius);
      });
    }
  }
  void move_member(int delta) {
    const int row = members->currentRow();
    const auto move = [&](auto& list) {
      const int next = row + delta;
      if (row >= 0 && next >= 0 && row < int(list.size()) && next < int(list.size())) {
        std::swap(list[row], list[next]);
        render_members();
        members->setCurrentRow(next);
        scope_changed();
      }
    };
    if (auto* g = active_group())
      move(g->edges);
    else if (auto* f = active_faces())
      move(f->faces);
    else
      move(constant_edges);
  }
  void remove_member() {
    const int row = members->currentRow();
    const auto remove = [&](auto& list) {
      if (row >= 0 && row < int(list.size())) {
        list.erase(list.begin() + row);
        render_members();
        scope_changed();
      }
    };
    if (auto* g = active_group())
      remove(g->edges);
    else if (auto* f = active_faces())
      remove(f->faces);
    else
      remove(constant_edges);
  }
  std::vector<std::string> selected_keys() {
    std::vector<std::string> result;
    if (auto* g = active_group())
      for (const auto& e : g->edges)
        result.push_back(interactive_edge_identity(e.edge));
    else if (auto* f = active_faces())
      for (const auto& r : f->faces)
        result.push_back(interactive_face_identity(r));
    else
      for (const auto& e : constant_edges)
        result.push_back(interactive_edge_identity(e));
    return result;
  }
  Result<std::size_t> capture(const std::vector<geometry::FinishingSelectionTarget>& available,
                              const std::vector<std::string>& selected) {
    std::vector<EdgeReference> edges;
    std::vector<FaceReference> faces;
    for (const auto& target : available) {
      std::visit(
          [&](const auto& r) {
            using T = std::decay_t<decltype(r)>;
            const auto key = [&] {
              if constexpr (std::is_same_v<T, EdgeReference>)
                return interactive_edge_identity(r);
              else
                return interactive_face_identity(r);
            }();
            labels[key] = QString::fromStdString(target.label);
            if (std::find(selected.begin(), selected.end(), target.semantic_id) != selected.end()) {
              if constexpr (std::is_same_v<T, EdgeReference>)
                edges.push_back(r);
              else
                faces.push_back(r);
            }
          },
          target.reference);
    }
    if (current_mode() < 2) {
      auto* g = active_group();
      auto& target = g ? g->edges : temporary_edges;
      temporary_edges.clear();
      if (!g)
        for (const auto& e : constant_edges)
          temporary_edges.push_back({e, false});
      for (const auto& e : edges)
        if (g)
          for (const auto& other : groups)
            if (other.id != g->id)
              for (const auto& old : other.edges)
                if (interactive_edge_identity(old.edge) == interactive_edge_identity(e))
                  return Result<std::size_t>::failure(
                      Error::validation(other.id, "Edge already belongs to another group"));
      target.erase(std::remove_if(target.begin(), target.end(),
                                  [&](const auto& old) {
                                    return std::none_of(
                                        edges.begin(), edges.end(), [&](const auto& e) {
                                          return interactive_edge_identity(old.edge) ==
                                                 interactive_edge_identity(e);
                                        });
                                  }),
                   target.end());
      for (const auto& e : edges)
        if (std::none_of(target.begin(), target.end(), [&](const auto& old) {
              return interactive_edge_identity(old.edge) == interactive_edge_identity(e);
            }))
          target.push_back({e, false});
      if (!g) {
        constant_edges.clear();
        for (const auto& e : target)
          constant_edges.push_back(e.edge);
      }
    } else {
      auto* target = active_faces();
      if (!target)
        return Result<std::size_t>::failure(Error::validation(id.value(), "Choose a face role"));
      for (const auto& r : faces)
        for (const auto* other : {&first, &second, &middle})
          if (other != target)
            for (const auto& old : other->faces)
              if (interactive_face_identity(old) == interactive_face_identity(r))
                return Result<std::size_t>::failure(
                    Error::validation(other->id, "Face already belongs to another role"));
      auto& list = target->faces;
      list.erase(
          std::remove_if(list.begin(), list.end(),
                         [&](const auto& old) {
                           return std::none_of(faces.begin(), faces.end(), [&](const auto& r) {
                             return interactive_face_identity(old) == interactive_face_identity(r);
                           });
                         }),
          list.end());
      for (const auto& r : faces)
        if (std::none_of(list.begin(), list.end(), [&](const auto& old) {
              return interactive_face_identity(old) == interactive_face_identity(r);
            }))
          list.push_back(r);
    }
    render_members();
    return Result<std::size_t>::success(edges.size() + faces.size());
  }
  void sync_values() {
    if (auto* g = active_group())
      for (int i = 0; i < int(g->stations.size()); ++i)
        if (auto* input = qobject_cast<QLineEdit*>(stations->cellWidget(i, 3));
            input && !input->hasFocus())
          input->setText(number(value(g->stations[i].radius)));
    if (!size.empty() && !size_value->hasFocus())
      size_value->setText(number(value(size)));
    if (policy && !setback_value->hasFocus())
      setback_value->setText(number(value(policy->distance)));
  }
  void remember_values() {
    if (live)
      for (const auto& [key, v] : live->values)
        values[key] = v;
  }
  void clear_inputs() {
    constant_edges.clear();
    for (auto& g : groups)
      g.edges.clear();
    first.faces.clear();
    second.faces.clear();
    middle.faces.clear();
    input_document.reset();
    input_cache.reset();
    input_prefix.reset();
    live.reset();
  }
  std::vector<FilletOrientedEdge> temporary_edges;
  Result<std::shared_ptr<GuiFilletDraft>> build() {
    if (live)
      for (const auto& [key, v] : live->values)
        values[key] = v;
    const int m = current_mode();
    const auto contact =
        continuity->currentIndex() == 1 ? FilletContinuity::Curvature : FilletContinuity::Tangent;
    Result<FilletFeature> record = [&]() {
      if (m == 0)
        return FilletFeature::create(id, name, body, constant_edges, size);
      if (m == 1) {
        VariableFilletIntent intent{groups, corner->isChecked() ? policy : std::nullopt};
        return FilletFeature::create_variable(id, name, body, intent);
      }
      if (m == 2)
        return FilletFeature::create_face_blend(
            id, name, body,
            FaceBlendIntent{first, second, contact,
                            contact == FilletContinuity::Tangent
                                ? FaceBlendSizeKind::Radius
                                : FaceBlendSizeKind::ContactSetback,
                            size, holds});
      return FilletFeature::create_full_round(
          id, name, body, FullRoundIntent{first, second, middle, contact, holds});
    }();
    if (!record)
      return Result<std::shared_ptr<GuiFilletDraft>>::failure(record.error());
    auto state = std::make_shared<GuiFilletDraft>(
        GuiFilletDraft{record.value(), editing, independent->isChecked(), {}, {}, {}, {}});
    const auto dependencies = record.value().parameter_dependencies();
    for (const auto& p : pending)
      if (std::find(dependencies.begin(), dependencies.end(), p.id()) != dependencies.end())
        state->pending.push_back(p);
    for (const auto& [key, v] : values)
      if (std::find(dependencies.begin(), dependencies.end(), ParameterId(key)) !=
          dependencies.end())
        state->values[key] = v;
    live = state;
    return Result<std::shared_ptr<GuiFilletDraft>>::success(std::move(state));
  }
};
} // namespace blcad::gui
