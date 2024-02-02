#include <metameric/core/uplifting.hpp>
#include <metameric/core/utility.hpp>
#include <nlohmann/json.hpp>

namespace met {
  bool UpliftingConstraint::operator==(const UpliftingConstraint &o) const {
    guard(type == o.type, false);
    if (type == Type::eColor) {
      return colr_i.isApprox(o.colr_i) 
        && rng::equal(colr_j, o.colr_j, eig::safe_approx_compare<Colr>)
        && rng::equal(csys_j, o.csys_j);
    } else if (type == Type::eColorOnMesh) {
      return colr_i.isApprox(o.colr_i) 
        && rng::equal(colr_j, o.colr_j, eig::safe_approx_compare<Colr>)
        && rng::equal(csys_j, o.csys_j);
    } else if (type == Type::eMeasurement) {
      return measurement.isApprox(o.measurement);
    }
    return false;
  }

  bool DirectColorConstraint::operator==(const DirectColorConstraint &o) const {
    return colr_i.isApprox(o.colr_i) 
      && rng::equal(colr_j, o.colr_j, eig::safe_approx_compare<Colr>)
      && rng::equal(csys_j, o.csys_j);
  }
  
  bool MeasurementConstraint::operator==(const MeasurementConstraint &o) const {
    return measurement.isApprox(o.measurement);
  }
  
  bool DirectSurfaceConstraint::operator==(const DirectSurfaceConstraint &o) const {
    return colr_i.isApprox(o.colr_i) 
      && rng::equal(colr_j, o.colr_j, eig::safe_approx_compare<Colr>)
      && rng::equal(csys_j, o.csys_j);
  }
  
  bool IndirectSurfaceConstraint::operator==(const IndirectSurfaceConstraint &o) const {
    return true;
  }

  void from_json(const json &js, DirectColorConstraint &c) {
    met_trace();
    js.at("is_active").get_to(c.is_active);
    js.at("colr_i").get_to(c.colr_i);
    js.at("colr_j").get_to(c.colr_j);
    js.at("csys_j").get_to(c.csys_j);
  }

  void to_json(json &js, const DirectColorConstraint &c) {
    met_trace();
    js = {{ "is_active", c.is_active },
          { "colr_i",    c.colr_i    },
          { "colr_j",    c.colr_j    },
          { "csys_j",    c.csys_j    }};
  }

  void from_json(const json &js, MeasurementConstraint &c) {
    met_trace();
    js.at("is_active").get_to(c.is_active);
    js.at("measurement").get_to(c.measurement);
  }

  void to_json(json &js, const MeasurementConstraint &c) {
    met_trace();
    js = {{ "is_active",   c.is_active   },
          { "measurement", c.measurement }};
  }

  void from_json(const json &js, DirectSurfaceConstraint &c) {
    met_trace();
    js.at("is_active").get_to(c.is_active);
    js.at("colr_i").get_to(c.colr_i);
    js.at("colr_j").get_to(c.colr_j);
    js.at("csys_j").get_to(c.csys_j);
    js.at("object_i").get_to(c.object_i);
    js.at("object_elem_i").get_to(c.object_elem_i);
    js.at("object_elem_bary").get_to(c.object_elem_bary);
  }

  void to_json(json &js, const DirectSurfaceConstraint &c) {
    met_trace();
    js = {{ "is_active",        c.is_active        },
          { "colr_i",           c.colr_i           },
          { "colr_j",           c.colr_j           },
          { "csys_j",           c.csys_j           },
          { "object_i",         c.object_i         },
          { "object_elem_i",    c.object_elem_i    },
          { "object_elem_bary", c.object_elem_bary }};
  }

  void from_json(const json &js, IndirectSurfaceConstraint &c) {
    met_trace();
    js.at("is_active").get_to(c.is_active);
    // TODO ...
  }

  void to_json(json &js, const IndirectSurfaceConstraint &c) {
    met_trace();
    js = {{ "is_active", c.is_active }};
    // TODO ...
  }
} // namespace met