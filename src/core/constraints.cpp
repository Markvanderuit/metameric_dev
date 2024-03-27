#include <metameric/core/ranges.hpp>
#include <metameric/core/constraints.hpp>
#include <metameric/core/utility.hpp>
#include <nlohmann/json.hpp>

namespace met {
  bool DirectColorConstraint::operator==(const DirectColorConstraint &o) const {
    return colr_i.isApprox(o.colr_i) 
      && rng::equal(colr_j, o.colr_j, eig::safe_approx_compare<Colr>)
      && rng::equal(csys_j, o.csys_j);
  }
  
  bool MeasurementConstraint::operator==(const MeasurementConstraint &o) const {
    return measure.isApprox(o.measure);
  }
  
  bool DirectSurfaceConstraint::operator==(const DirectSurfaceConstraint &o) const {
    return surface == o.surface
        && rng::equal(colr_j, o.colr_j, eig::safe_approx_compare<Colr>)
        && rng::equal(csys_j, o.csys_j);
  }
  
  bool IndirectSurfaceConstraint::operator==(const IndirectSurfaceConstraint &o) const {
    return surface == o.surface
        && rng::equal(colr, o.colr, eig::safe_approx_compare<Colr>)
        && rng::equal(powers, o.powers, eig::safe_approx_compare<Spec>);
  }

  /* bool _IndirectSurfaceConstraint::Constraint::operator==(const Constraint &o) const {
    return surface == o.surface && csys == o.csys && colr.isApprox(o.colr);
  }

  bool _IndirectSurfaceConstraint::Constraint::is_valid() const {
    return surface.is_valid() && surface.record.is_object();
  }

  bool _IndirectSurfaceConstraint::Constraint::has_mismatching() const {
    return !csys.powers.empty() && !colr.isZero();
  }

  bool _IndirectSurfaceConstraint::operator==(const _IndirectSurfaceConstraint &o) const {
    return rng::equal(constraints, o.constraints);
  }

  bool _IndirectSurfaceConstraint::is_valid() const {
    return rng::all_of(constraints, [](const auto &c) { return c.is_valid(); });
  }

  bool _IndirectSurfaceConstraint::has_mismatching() const {
    return rng::all_of(constraints, [](const auto &c) { return c.has_mismatching(); });
  } */

  void from_json(const json &js, DirectColorConstraint &c) {
    met_trace();
    js.at("colr_i").get_to(c.colr_i);
    js.at("colr_j").get_to(c.colr_j);
    js.at("csys_j").get_to(c.csys_j);
  }

  void to_json(json &js, const DirectColorConstraint &c) {
    met_trace();
    js = {{ "colr_i", c.colr_i },
          { "colr_j", c.colr_j },
          { "csys_j", c.csys_j }};
  }

  void from_json(const json &js, MeasurementConstraint &c) {
    met_trace();
    js.at("measurement").get_to(c.measure);
  }

  void to_json(json &js, const MeasurementConstraint &c) {
    met_trace();
    js = {{ "measurement", c.measure }};
  }

  void from_json(const json &js, DirectSurfaceConstraint &c) {
    met_trace();
    js.at("colr_j").get_to(c.colr_j);
    js.at("csys_j").get_to(c.csys_j);
    js.at("surface").get_to(c.surface);
  }

  void to_json(json &js, const DirectSurfaceConstraint &c) {
    met_trace();
    js = {{ "colr_j",  c.colr_j  },
          { "csys_j",  c.csys_j  },
          { "surface", c.surface }};
  }

  void from_json(const json &js, IndirectSurfaceConstraint &c) {
    met_trace();
    js.at("colr").get_to(c.colr);
    js.at("powers").get_to(c.powers);
    js.at("surface").get_to(c.surface);
  }

  void to_json(json &js, const IndirectSurfaceConstraint &c) {
    met_trace();
    js = {{ "colr",    c.colr    },
          { "powers",  c.powers  },
          { "surface", c.surface }};
  }
} // namespace met