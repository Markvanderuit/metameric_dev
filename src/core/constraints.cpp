#include <metameric/core/ranges.hpp>
#include <metameric/core/constraints.hpp>
#include <metameric/core/components.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/scene.hpp>
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
    return !cstr_j.empty() && detail::has_duplicates(cstr_j) && has_surface();
  }

  bool IndirectSurfaceConstraint::has_mismatching() const { 
    return !powers.empty() && !colr.isZero() && !colr.isOnes() && has_surface();
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

  Colr MeasurementConstraint::position(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();
    CMFS csys = scene.csys(uplifting.csys_i).finalize();
    return (csys.transpose() * measure.matrix()).eval();
  }

  Spec DirectColorConstraint::realize(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();

    // Gather all relevant color system spectra and corresponding color signals
    DirectSpectrumInfo spec_info = {
      .direct_constraints = {{ scene.csys(uplifting.csys_i), colr_i }},
      .basis              = scene.resources.bases[uplifting.basis_i].value()
    };
    for (const auto &c : cstr_j)
      spec_info.direct_constraints.push_back({ scene.csys(c.cmfs_j, c.illm_j), c.colr_j });

    // Generate a metamer satisfying the system+signal constraint set and return as pair
    return generate_spectrum(spec_info);
  }

  Spec DirectSurfaceConstraint::realize(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();

    // Return zero constraint for invalid surfaces
    guard(has_surface(), Spec(0));

    // Gather all relevant color system spectra and corresponding color signals
    DirectSpectrumInfo spec_info = {
      .direct_constraints = {{ scene.csys(uplifting.csys_i), colr_i }},
      .basis              = scene.resources.bases[uplifting.basis_i].value()
    };
    for (const auto &c : cstr_j)
      spec_info.direct_constraints.push_back({ scene.csys(c.cmfs_j, c.illm_j), c.colr_j });

    // Generate a metamer satisfying the system+signal constraint set and return as pair
    return generate_spectrum(spec_info);
  }

  Spec IndirectSurfaceConstraint::realize(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();
    
    // Return zero constraint for invalid surfaces
    guard(has_surface(), Spec(0));

    if (has_mismatching()) {
      // Gather all relevant color system spectra and corresponding color signals
      IndirectColrSystem csys = {
        .cmfs   = scene.resources.observers[scene.components.observer_i.value].value(),
        .powers = powers
      };
      IndrctSpectrumInfo spec_info = {
        .direct_constraints   = {{ scene.csys(uplifting.csys_i), surface.diffuse }},
        .indirect_constraints = {{ csys, colr }},
        .basis                = scene.resources.bases[uplifting.basis_i].value()
      };

      return generate_spectrum(spec_info);
    } else { // We attempt to fill in a default spectrum, which is necessary to establish the initial system
      // Gather all relevant color system spectra and corresponding color signals
      DirectSpectrumInfo spec_info = {
        .direct_constraints = {{ scene.csys(uplifting.csys_i), surface.diffuse }},
        .basis              = scene.resources.bases[uplifting.basis_i].value()
      };

      // Generate a metamer satisfying the system+signal constraint set and return as pair
      return generate_spectrum(spec_info);
    }
  }

  std::vector<Colr> DirectColorConstraint::realize_mismatching(const Scene &scene, const Uplifting &uplifting, uint csys_i, uint seed, uint samples) const {
    met_trace();
    
    // Assemble info object for generating boundary spectra
    DirectMismatchingOCSInfo ocs_info = {
      .basis     = scene.resources.bases[uplifting.basis_i].value(),
      .seed      = seed,
      .n_samples = samples
    };

    // Specify pair of direct color systems forming objective
    ocs_info.direct_objectives = {
      scene.csys(uplifting.csys_i),
      scene.csys(cstr_j[csys_i].cmfs_j, cstr_j[csys_i].illm_j)
    };

    // Specify direct color constraints
    ocs_info.direct_constraints.push_back({ ocs_info.direct_objectives[0], colr_i });
    for (uint i = 0; i < cstr_j.size(); ++i) {
      guard_continue(i != csys_i);
      const auto &c = cstr_j[i];
      ocs_info.direct_constraints.push_back({ scene.csys(c.cmfs_j, c.illm_j), c.colr_j });
    }

    // Output color values
    return ocs_info.direct_objectives[1](generate_mismatching_ocs(ocs_info));
  }

  std::vector<Colr> DirectSurfaceConstraint::realize_mismatching(const Scene &scene, const Uplifting &uplifting, uint csys_i, uint seed, uint samples) const {
    met_trace();
    
    // Assemble info object for generating boundary spectra
    DirectMismatchingOCSInfo ocs_info = {
      .basis     = scene.resources.bases[uplifting.basis_i].value(),
      .seed      = seed,
      .n_samples = samples
    };

    // Specify pair of direct color systems forming objective
    ocs_info.direct_objectives = {
      scene.csys(uplifting.csys_i),
      scene.csys(cstr_j[csys_i].cmfs_j, cstr_j[csys_i].illm_j)
    };

    // Specify direct color constraints
    ocs_info.direct_constraints.push_back({ ocs_info.direct_objectives[0], colr_i });
    for (uint i = 0; i < cstr_j.size(); ++i) {
      guard_continue(i != csys_i);
      const auto &c = cstr_j[i];
      ocs_info.direct_constraints.push_back({ scene.csys(c.cmfs_j, c.illm_j), c.colr_j });
    }

    // Output color values
    return ocs_info.direct_objectives[1](generate_mismatching_ocs(ocs_info));
  }

  std::vector<Colr> IndirectSurfaceConstraint::realize_mismatching(const Scene &scene, const Uplifting &uplifting, uint csys_i, uint seed, uint samples) const {
    met_trace();

    // Construct indirect color system using scene observer
    IndirectColrSystem csys = {
      .cmfs   = scene.resources.observers[scene.components.observer_i.value].value(),
      .powers = powers
    };

    // Assemble info object for generating boundary spectra
    IndirectMismatchingOCSInfo ocs_info = {
      .basis     = scene.resources.bases[uplifting.basis_i].value(),
      .seed      = seed,
      .n_samples = samples
    };

    // Specify relevant objectives and constraints
    ocs_info.direct_objective   = scene.csys(uplifting.csys_i);
    ocs_info.indirect_objective = csys;
    ocs_info.direct_constraints = {{ ocs_info.direct_objective, surface.diffuse }};

    // Output color values
    return ocs_info.indirect_objective(generate_mismatching_ocs(ocs_info));
  }
} // namespace met