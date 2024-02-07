#include <metameric/core/uplifting.hpp>
#include <metameric/core/utility.hpp>
#include <nlohmann/json.hpp>

namespace met {
  bool DirectColorConstraint::operator==(const DirectColorConstraint &o) const {
    return colr_i.isApprox(o.colr_i) 
      && rng::equal(colr_j, o.colr_j, eig::safe_approx_compare<Colr>)
      && rng::equal(csys_j, o.csys_j);
  }
  
  bool MeasurementConstraint::operator==(const MeasurementConstraint &o) const {
    return measurement.isApprox(o.measurement);
  }
  
  bool DirectSurfaceConstraint::operator==(const DirectSurfaceConstraint &o) const {
    return surface == o.surface
        && rng::equal(colr_j, o.colr_j, eig::safe_approx_compare<Colr>)
        && rng::equal(csys_j, o.csys_j);
  }
  
  bool IndirectSurfaceConstraint::operator==(const IndirectSurfaceConstraint &o) const {
    return true;
  }

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
    js.at("measurement").get_to(c.measurement);
  }

  void to_json(json &js, const MeasurementConstraint &c) {
    met_trace();
    js = {{ "measurement", c.measurement }};
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
    // TODO ...
  }

  void to_json(json &js, const IndirectSurfaceConstraint &c) {
    met_trace();
    // TODO ...
  }
} // namespace met