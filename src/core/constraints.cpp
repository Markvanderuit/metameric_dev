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
        for (auto j = i + 1; j != r.end(); ++j) {
          if (*i == *j)
            return true;
        }
      return false;
    }
  } // namespace detail

  bool ColrConstraint::operator==(const ColrConstraint &o) const {
    return cmfs_j == o.cmfs_j && illm_j == o.illm_j && colr_j.isApprox(o.colr_j);
  }

  bool ColrConstraint::is_similar(const ColrConstraint &o) const {
    return cmfs_j == o.cmfs_j && illm_j == o.illm_j;
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

  bool DirectColorConstraint::has_mismatching(const Scene &scene, const Uplifting &uplifting) const { 
    // Merge all known color system data
    auto cstr_i = scene.components.colr_systems[uplifting.csys_i].value;
    auto cstr = cstr_j
              | vws::transform([](const auto &v) { return std::pair { v.cmfs_j, v.illm_j }; })
              | rng::to<std::vector>();
    cstr.push_back({ cstr_i.observer_i, cstr_i.illuminant_i });
    
    // Mismatching only occurs if there are two or more color systems, and all are unique
    return cstr.size() > 1 && !detail::has_duplicates(cstr);
  }

  bool DirectSurfaceConstraint::has_mismatching(const Scene &scene, const Uplifting &uplifting) const { 
    // Merge all known color system data
    auto cstr_i = scene.components.colr_systems[uplifting.csys_i].value;
    auto cstr = cstr_j
              | vws::transform([](const auto &v) { return std::pair { v.cmfs_j, v.illm_j }; })
              | rng::to<std::vector>();
    cstr.push_back({ cstr_i.observer_i, cstr_i.illuminant_i });
    
    // Mismatching only occurs if there are two or more color systems, and all are unique
    return has_surface() && cstr.size() > 1 && !detail::has_duplicates(cstr);
  }

  bool IndirectSurfaceConstraint::has_mismatching(const Scene &scene, const Uplifting &uplifting) const { 
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
    return scene.csys(uplifting.csys_i)(measure);
  }
  
  std::pair<Spec, Basis::vec_type> MeasurementConstraint::realize(const Scene &scene, const Uplifting &uplifting) const { 
    auto basis = scene.resources.bases[uplifting.basis_i].value();
    SpectrumCoeffsInfo spec_info = { .spec = measure, .basis = basis };
    return generate_spectrum(spec_info); // fit basis to reproduce measure
    // return { measure, Basis::vec_type(0) }; 
  }

  std::pair<Spec, Basis::vec_type> DirectColorConstraint::realize(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();

    // Gather all relevant color system spectra and corresponding color signals
    auto basis = scene.resources.bases[uplifting.basis_i].value();
    DirectSpectrumInfo spec_info = {
      .direct_constraints = {{ scene.csys(uplifting.csys_i), colr_i }},
      .basis              = basis
    };
    for (const auto &c : cstr_j)
      spec_info.direct_constraints.push_back({ scene.csys(c.cmfs_j, c.illm_j), c.colr_j });

    // Generate a metamer satisfying the system+signal constraint set and return as pair
    return generate_spectrum(spec_info);
  }

  std::pair<Spec, Basis::vec_type> DirectSurfaceConstraint::realize(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();

    // Return zero constraint for invalid surfaces
    guard(has_surface(), { Spec(0), Basis::vec_type(0) });

    // Gather all relevant color system spectra and corresponding color signals
    auto basis = scene.resources.bases[uplifting.basis_i].value();
    DirectSpectrumInfo spec_info = {
      .direct_constraints = {{ scene.csys(uplifting.csys_i), colr_i }},
      .basis              = basis
    };
    for (const auto &c : cstr_j)
      spec_info.direct_constraints.push_back({ scene.csys(c.cmfs_j, c.illm_j), c.colr_j });

    // Generate a metamer satisfying the system+signal constraint set and return as pair
    return generate_spectrum(spec_info);
  }

  std::pair<Spec, Basis::vec_type> IndirectSurfaceConstraint::realize(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();
    
    // Return zero constraint for invalid surfaces
    guard(has_surface(), { Spec(0), Basis::vec_type(0) });

    if (has_mismatching(scene, uplifting)) {
      // Gather all relevant color system spectra and corresponding color signals
      auto basis = scene.resources.bases[uplifting.basis_i].value();
      IndirectColrSystem csys = {
        .cmfs   = scene.resources.observers[scene.components.observer_i.value].value(),
        .powers = powers
      };
      IndirectSpectrumInfo spec_info = {
        .direct_constraints   = {{ scene.csys(uplifting.csys_i), surface.diffuse }},
        .indirect_constraints = {{ csys, colr }},
        .basis                = basis
      };

      // Generate a metamer satisfying the system+signal constraint set and return as pair
      return generate_spectrum(spec_info);
    } else { // We attempt to fill in a default spectrum, which is necessary to establish the initial system
      // Gather all relevant color system spectra and corresponding color signals
      auto basis = scene.resources.bases[uplifting.basis_i].value();
      DirectSpectrumInfo spec_info = {
        .direct_constraints = {{ scene.csys(uplifting.csys_i), surface.diffuse }},
        .basis              = basis
      };

      // Generate a metamer satisfying the system+signal constraint set and return as pair
      return generate_spectrum(spec_info);
    }
  }

  std::vector<std::tuple<Colr, Spec, Basis::vec_type>> DirectColorConstraint::realize_mismatching(const Scene &scene, const Uplifting &uplifting, uint csys_i, uint seed, uint samples) const {
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
    return generate_mismatching_ocs(ocs_info);
  }

  std::vector<std::tuple<Colr, Spec, Basis::vec_type>> DirectSurfaceConstraint::realize_mismatching(const Scene &scene, const Uplifting &uplifting, uint csys_i, uint seed, uint samples) const {
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
    return generate_mismatching_ocs(ocs_info);
  }

  std::vector<std::tuple<Colr, Spec, Basis::vec_type>> IndirectSurfaceConstraint::realize_mismatching(const Scene &scene, const Uplifting &uplifting, uint csys_i, uint seed, uint samples) const {
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
    return generate_mismatching_ocs(ocs_info);
  }
} // namespace met