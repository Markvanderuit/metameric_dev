#include <metameric/core/ranges.hpp>
#include <metameric/core/constraints.hpp>
#include <metameric/core/utility.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>

namespace met {
  namespace detail {
    bool has_duplicates(const rng::range auto &r) {
      for (auto i = r.begin(); i != r.end(); ++i)
        for (auto j = r.begin(); j != r.end(); ++j)
          if (*i == *j)
            return true;
      return false;
    }
  } // namespace detail

  bool ColrConstraint::operator==(const ColrConstraint &o) const {
    return cmfs_j == o.cmfs_j && illm_j == o.illm_j && colr_j.isApprox(o.colr_j);
  }

  bool IndirectConstraint::operator==(const IndirectConstraint &o) const {
    return cmfs_i == o.cmfs_i
        && illm_i == o.illm_i
        && colr_i.isApprox(o.colr_i)
        && colr_j.isApprox(o.colr_j) 
        && rng::equal(pwrs_j, o.pwrs_j, eig::safe_approx_compare<Spec>);
  }

  bool DirectColorConstraint::operator==(const DirectColorConstraint &o) const {
    return colr_i.isApprox(o.colr_i) && rng::equal(cstr_j, o.cstr_j, [](const auto &a, const auto &b) {
      return a.cmfs_j == b.cmfs_j && a.illm_j == b.illm_j && a.colr_j.isApprox(b.colr_j);
    });
  }
  
  bool MeasurementConstraint::operator==(const MeasurementConstraint &o) const {
    return measure.isApprox(o.measure);
  }
  
  bool DirectSurfaceConstraint::operator==(const DirectSurfaceConstraint &o) const {
    return surface == o.surface 
        && colr_i.isApprox(o.colr_i) 
        && rng::equal(cstr_j, o.cstr_j, [](const auto &a, const auto &b) {
            return a.cmfs_j == b.cmfs_j && a.illm_j == b.illm_j && a.colr_j.isApprox(b.colr_j);
          });
  }
  
  bool IndirectSurfaceConstraint::operator==(const IndirectSurfaceConstraint &o) const {
    return surface == o.surface
        && rng::equal(colr, o.colr, eig::safe_approx_compare<Colr>)
        && rng::equal(powers, o.powers, eig::safe_approx_compare<Spec>);
  }

  bool DirectColorConstraint::has_mismatching() const { 
    return !cstr_j.empty() && detail::has_duplicates(cstr_j);
  }

  bool DirectSurfaceConstraint::has_mismatching() const { 
    return !cstr_j.empty() && detail::has_duplicates(cstr_j);
  }

  bool IndirectSurfaceConstraint::has_mismatching() const { 
    return !powers.empty() && !colr.isZero() && !colr.isOnes(); 
  }

  void from_json(const json &js, DirectColorConstraint &c) {
    met_trace();
    js.at("colr_i").get_to(c.colr_i);
    js.at("cstr_j").get_to(c.cstr_j);
  }

  void from_json(const json &js, ColrConstraint &c) {
    met_trace();
    js.at("cmfs_j").get_to(c.cmfs_j);
    js.at("illm_j").get_to(c.illm_j);
    js.at("colr_j").get_to(c.colr_j);
  }

  void from_json(const json &js, IndirectConstraint &c) {
    met_trace();
    js.at("cmfs_i").get_to(c.cmfs_i);
    js.at("illm_i").get_to(c.illm_i);
    js.at("colr_i").get_to(c.colr_i);
    js.at("pwrs_j").get_to(c.pwrs_j);
    js.at("colr_j").get_to(c.colr_j);
  }

  void to_json(json &js, const DirectColorConstraint &c) {
    met_trace();
    js = {{ "colr_i", c.colr_i },
          { "cstr_j", c.cstr_j }};
  }

  void to_json(json &js, const ColrConstraint &c) {
    met_trace();
    js = {{ "cmfs_j", c.cmfs_j },
          { "illm_j", c.illm_j },
          { "colr_j", c.colr_j }};
  }

  void to_json(json &js, const IndirectConstraint &c) {
    met_trace();
    js = {{ "cmfs_i", c.cmfs_i },
          { "illm_i", c.illm_i },
          { "colr_i", c.colr_i },
          { "colr_j", c.colr_j },
          { "pwrs_j", c.pwrs_j }};
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
    js.at("colr_i").get_to(c.colr_i);
    js.at("cstr_j").get_to(c.cstr_j);
    js.at("surface").get_to(c.surface);
  }

  void to_json(json &js, const DirectSurfaceConstraint &c) {
    met_trace();
    js = {{ "colr_i",  c.colr_i  },
          { "cstr_j",  c.cstr_j  },
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