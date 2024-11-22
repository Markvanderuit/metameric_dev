#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/scene/scene.hpp>
#include <metameric/scene/constraints.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>

namespace met {
  bool LinearConstraint::operator==(const LinearConstraint &o) const {
    return is_active == o.is_active 
        && cmfs_j == o.cmfs_j 
        && illm_j == o.illm_j 
        && colr_j.isApprox(o.colr_j);
  }

  bool LinearConstraint::is_similar(const LinearConstraint &o) const {
    return is_active == o.is_active 
        && cmfs_j == o.cmfs_j 
        && illm_j == o.illm_j;
  }

  bool NLinearConstraint::operator==(const NLinearConstraint &o) const {
    return is_active == o.is_active
        && cmfs_j == o.cmfs_j
        && rng::equal(powr_j, o.powr_j, eig::safe_approx_compare<Spec>)
        && colr_j.isApprox(o.colr_j);
  }

  bool NLinearConstraint::is_similar(const NLinearConstraint &o) const {
    return is_active == o.is_active
        && cmfs_j == o.cmfs_j
        && rng::equal(powr_j, o.powr_j, eig::safe_approx_compare<Spec>);
  }

  bool DirectColorConstraint::operator==(const DirectColorConstraint &o) const {
    return is_base_active == o.is_base_active
        && colr_i.isApprox(o.colr_i) 
        && rng::equal(cstr_j, o.cstr_j);
  }
  
  bool MeasurementConstraint::operator==(const MeasurementConstraint &o) const {
    return measure.isApprox(o.measure);
  }
  
  bool DirectSurfaceConstraint::operator==(const DirectSurfaceConstraint &o) const {
    return is_base_active == o.is_base_active
        && surface == o.surface 
        && colr_i.isApprox(o.colr_i) 
        && rng::equal(cstr_j, o.cstr_j);
  }
  
  bool IndirectSurfaceConstraint::operator==(const IndirectSurfaceConstraint &o) const {
    return is_base_active == o.is_base_active
        && colr_i.isApprox(o.colr_i) 
        && rng::equal(cstr_j,   o.cstr_j)
        && rng::equal(surfaces, o.surfaces);
  }

  void from_json(const json &js, DirectColorConstraint &c) {
    met_trace();
    js.at("is_base_active").get_to(c.is_base_active);
    js.at("colr_i").get_to(c.colr_i);
    js.at("cstr_j").get_to(c.cstr_j);
  }

  void from_json(const json &js, LinearConstraint &c) {
    met_trace();
    js.at("is_active").get_to(c.is_active);
    js.at("cmfs_j").get_to(c.cmfs_j);
    js.at("illm_j").get_to(c.illm_j);
    js.at("colr_j").get_to(c.colr_j);
  }

  void from_json(const json &js, NLinearConstraint &c) {
    met_trace();
    js.at("is_active").get_to(c.is_active);
    js.at("cmfs_j").get_to(c.cmfs_j);
    js.at("powr_j").get_to(c.powr_j);
    js.at("colr_j").get_to(c.colr_j);
  }

  void to_json(json &js, const LinearConstraint &c) {
    met_trace();
    js = {{ "is_active", c.is_active },
          { "cmfs_j",    c.cmfs_j    },
          { "illm_j",    c.illm_j    },
          { "colr_j",    c.colr_j    }};
  }

  void to_json(json &js, const NLinearConstraint &c) {
    met_trace();
    js = {{ "is_active", c.is_active },
          { "cmfs_j",    c.cmfs_j    },
          { "powr_j",    c.powr_j    },
          { "colr_j",    c.colr_j    }};
  }

  void to_json(json &js, const DirectColorConstraint &c) {
    met_trace();
    js = {{ "is_base_active", c.is_base_active },
          { "colr_i",         c.colr_i         },
          { "cstr_j",         c.cstr_j         }};
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
    js.at("is_base_active").get_to(c.is_base_active);
    js.at("colr_i").get_to(c.colr_i);
    js.at("cstr_j").get_to(c.cstr_j);
    js.at("surface").get_to(c.surface);
  }

  void to_json(json &js, const DirectSurfaceConstraint &c) {
    met_trace();
    js = {{ "is_base_active", c.is_base_active },
          { "colr_i",         c.colr_i         },
          { "cstr_j",         c.cstr_j         },
          { "surface",        c.surface        }};
  }


  struct OldNLinearConstraint {
    SurfaceInfo surface;
  };

  void from_json(const json &js, OldNLinearConstraint &c) {
    met_trace();
    js.at("surface").get_to(c.surface);
  }

  void from_json(const json &js, IndirectSurfaceConstraint &c) {
    met_trace();
    js.at("is_base_active").get_to(c.is_base_active);
    js.at("colr_i").get_to(c.colr_i);

    // Hotfix for now to update scenes
    if (js.contains("cstr_j_indrct")) {
      std::vector<OldNLinearConstraint> old;
      js.at("cstr_j_indrct").get_to(c.cstr_j);
      js.at("cstr_j_indrct").get_to(old);
      rng::transform(old, std::back_inserter(c.surfaces), &OldNLinearConstraint::surface);
    } else {
      js.at("cstr_j").get_to(c.cstr_j);
      js.at("surfaces").get_to(c.surfaces);
    }
  }

  void to_json(json &js, const IndirectSurfaceConstraint &c) {
    met_trace();
    js = {{ "is_base_active", c.is_base_active },
          { "colr_i",         c.colr_i         },
          { "cstr_j",         c.cstr_j         },
          { "surfaces",       c.surfaces       }};
  }

  SpectrumSample MeasurementConstraint::realize(const Scene &scene, const Uplifting &uplifting) const { 
    auto basis = scene.resources.bases[uplifting.basis_i].value();
    SpectrumCoeffsInfo spec_info = { .spec = measure, .basis = basis };
    return solve_spectrum(spec_info); // fit basis to reproduce measure
    // return { measure, Basis::vec_type(0) }; 
  }

  SpectrumSample DirectColorConstraint::realize(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();

    // Gather all relevant color system spectra and corresponding color signals
    auto basis = scene.resources.bases[uplifting.basis_i].value();
    DirectSpectrumInfo spec_info = {
      .linear_constraints = {{ scene.csys(uplifting), colr_i }},
      .basis              = basis
    };
    for (const auto &c : cstr_j)
      spec_info.linear_constraints.push_back({ scene.csys(c.cmfs_j, c.illm_j), c.colr_j });

    // Generate a metamer satisfying the system+signal constraint set and return as pair
    return solve_spectrum(spec_info);
  }

  SpectrumSample DirectSurfaceConstraint::realize(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();

    // Return zero constraint for black
    guard(!colr_i.isZero(), { Spec(0), Basis::vec_type(0) });

    // Gather all relevant color system spectra and corresponding color signals
    auto basis = scene.resources.bases[uplifting.basis_i].value();
    DirectSpectrumInfo spec_info = {
      .linear_constraints = {{ scene.csys(uplifting), colr_i }},
      .basis              = basis
    };
    for (const auto &c : cstr_j)
      spec_info.linear_constraints.push_back({ scene.csys(c.cmfs_j, c.illm_j), c.colr_j });

    // Generate a metamer satisfying the system+signal constraint set and return as pair
    return solve_spectrum(spec_info);
  }

  SpectrumSample IndirectSurfaceConstraint::realize(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();
    
    // Return zero constraint for black
    guard(!colr_i.isZero(), { Spec(0), Basis::vec_type(0) });

    /* if (has_mismatching(scene, uplifting)) {
      // NOTE; not implemented r.n. as gen_uplifting_task overrides this behavior with a nice, 
      //       mismatch-volume based approximation. See MetamerConstraintBuilder

      auto basis = scene.resources.bases[uplifting.basis_i].value();

      // Gather all relevant color system spectra and corresponding color signals
      IndirectColrSystem csys = {
        .cmfs   = scene.resources.observers[scene.components.observer_i.value].value(),
        .powers = powers
      };
      IndirectSpectrumInfo spec_info = {
        .linear_constraints   = {{ scene.csys(uplifting.csys_i), surface.diffuse }},
        .nlinear_constraints = {{ csys, colr }},
        .basis                = basis
      };

      // Generate a metamer satisfying the system+signal constraint set and return as pair
      return solve_spectrum(spec_info);
    } else { */ 
      // We attempt to fill in a default spectrum, which is necessary to establish the initial system
      // Gather all relevant color system spectra and corresponding color signals
      
      auto basis = scene.resources.bases[uplifting.basis_i].value();
      DirectSpectrumInfo spec_info = {
        .linear_constraints = {{ scene.csys(uplifting), colr_i }},
        .basis              = basis
      };

      // Generate a metamer satisfying the system+signal constraint set and return as pair
      return solve_spectrum(spec_info);
    // }
  }

  std::vector<MismatchSample> DirectColorConstraint::realize_mismatch(const Scene &scene, const Uplifting &uplifting, uint seed, uint samples) const {
    met_trace();
    
    // Filter out inactive constraints
    auto direct_cstr = cstr_j | vws::filter(&LinearConstraint::is_active) | view_to<std::vector<LinearConstraint>>();

    // Assemble info object for generating boundary spectra
    DirectMismatchSolidInfo info = {
      .basis     = scene.resources.bases[uplifting.basis_i].value(),
      .seed      = seed,
      .n_samples = samples
    };

    // Base roundtrip objective
    if (is_base_active)
      info.linear_objectives.push_back(scene.csys(uplifting));

    // Specify direct color systems forming objective
    rng::transform(direct_cstr, 
      std::back_inserter(info.linear_objectives),
      [&](const auto &c) { return scene.csys(c.cmfs_j, c.illm_j); });

    // Base roundtrip constraint
    if (is_base_active)
      info.linear_constraints.push_back({ scene.csys(uplifting), colr_i });

    // Specify direct color constraints; all but the last constraint (the "free variable") are specified
    rng::transform(direct_cstr | vws::take(direct_cstr.size() - 1), 
      std::back_inserter(info.linear_constraints),
      [&](const auto &c) { return std::pair { scene.csys(c.cmfs_j, c.illm_j), c.colr_j }; });

    return solve_mismatch_solid(info);
  }

  std::vector<MismatchSample> DirectSurfaceConstraint::realize_mismatch(const Scene &scene, const Uplifting &uplifting, uint seed, uint samples) const {
    met_trace();
    
    // Filter out inactive constraints
    auto direct_cstr = cstr_j | vws::filter(&LinearConstraint::is_active) |  view_to<std::vector<LinearConstraint>>();

    // Assemble info object for generating boundary spectra
    DirectMismatchSolidInfo info = {
      .basis     = scene.resources.bases[uplifting.basis_i].value(),
      .seed      = seed,
      .n_samples = samples
    };

    // Base roundtrip objective
    if (is_base_active)
      info.linear_objectives.push_back(scene.csys(uplifting));

    // Specify direct color systems forming objective
    rng::transform(direct_cstr, 
      std::back_inserter(info.linear_objectives),
      [&](const auto &c) { return scene.csys(c.cmfs_j, c.illm_j); });

    // Base roundtrip constraint
    if (is_base_active)
      info.linear_constraints.push_back({ scene.csys(uplifting), colr_i });

    // Specify direct color constraints; all but the last constraint (the "free variable") are specified
    rng::transform(direct_cstr | vws::take(direct_cstr.size() - 1), 
      std::back_inserter(info.linear_constraints),
      [&](const auto &c) { return std::pair { scene.csys(c.cmfs_j, c.illm_j), c.colr_j }; });

    return solve_mismatch_solid(info);
  }

  std::vector<MismatchSample> IndirectSurfaceConstraint::realize_mismatch(const Scene &scene, const Uplifting &uplifting, uint seed, uint samples) const {
    met_trace();

    // Filter out inactive constraints
    auto indrct_cstr = cstr_j | vws::filter(&NLinearConstraint::is_active) |  view_to<std::vector<NLinearConstraint>>();

    // Assemble info object for generating boundary spectra
    IndirectMismatchSolidInfo info = {
      .basis     = scene.resources.bases[uplifting.basis_i].value(),
      .seed      = seed,
      .n_samples = samples
    };

    // Specify indirect color systems forming objective
    info.nlinear_objectives.push_back({
      .cmfs   = scene.resources.observers[indrct_cstr.back().cmfs_j].value(),
      .powers = indrct_cstr.back().powr_j
    });

    // Base roundtrip constraint
    if (is_base_active)
      info.linear_constraints.push_back({ scene.csys(uplifting), colr_i });
      
    // Specify direct/indirect color constraints; all but the last constraint (the "free variable") are specified
    rng::transform(indrct_cstr | vws::take(indrct_cstr.size() - 1), 
      std::back_inserter(info.nlinear_constraints),
      [&](const auto &c) { 
        return std::pair { IndirectColrSystem { scene.resources.observers[c.cmfs_j].value(), c.powr_j }, c.colr_j }; 
      });

    // Output color values
    return solve_mismatch_solid(info);
  }
} // namespace met