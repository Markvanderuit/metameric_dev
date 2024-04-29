#include <metameric/core/ranges.hpp>
#include <metameric/core/constraints.hpp>
#include <metameric/core/components.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>

namespace met {
  bool ColrConstraint::operator==(const ColrConstraint &o) const {
    return is_active == o.is_active 
        && cmfs_j == o.cmfs_j 
        && illm_j == o.illm_j 
        && colr_j.isApprox(o.colr_j);
  }

  bool ColrConstraint::is_similar(const ColrConstraint &o) const {
    return is_active == o.is_active 
        && cmfs_j == o.cmfs_j 
        && illm_j == o.illm_j;
  }

  bool PowrConstraint::operator==(const PowrConstraint &o) const {
    return is_active == o.is_active
        && cmfs_j == o.cmfs_j
        && rng::equal(powr_j, o.powr_j, eig::safe_approx_compare<Spec>)
        && colr_j.isApprox(o.colr_j)
        && surface == o.surface;
  }

  bool PowrConstraint::is_similar(const PowrConstraint &o) const {
    return is_active == o.is_active
        && cmfs_j == o.cmfs_j
        && rng::equal(powr_j, o.powr_j, eig::safe_approx_compare<Spec>)
        && surface == o.surface;
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
        && target_direct == o.target_direct
        && rng::equal(cstr_j_direct, o.cstr_j_direct)
        && rng::equal(cstr_j_indrct, o.cstr_j_indrct);
  }

  void from_json(const json &js, DirectColorConstraint &c) {
    met_trace();
    js.at("is_base_active").get_to(c.is_base_active);
    js.at("colr_i").get_to(c.colr_i);
    js.at("cstr_j").get_to(c.cstr_j);
  }

  void from_json(const json &js, ColrConstraint &c) {
    met_trace();
    js.at("is_active").get_to(c.is_active);
    js.at("cmfs_j").get_to(c.cmfs_j);
    js.at("illm_j").get_to(c.illm_j);
    js.at("colr_j").get_to(c.colr_j);
  }

  void from_json(const json &js, PowrConstraint &c) {
    met_trace();
    js.at("is_active").get_to(c.is_active);
    js.at("cmfs_j").get_to(c.cmfs_j);
    js.at("powr_j").get_to(c.powr_j);
    js.at("colr_j").get_to(c.colr_j);
    js.at("surface").get_to(c.surface);
  }

  void to_json(json &js, const ColrConstraint &c) {
    met_trace();
    js = {{ "is_active", c.is_active },
          { "cmfs_j",    c.cmfs_j    },
          { "illm_j",    c.illm_j    },
          { "colr_j",    c.colr_j    }};
  }

  void to_json(json &js, const PowrConstraint &c) {
    met_trace();
    js = {{ "is_active", c.is_active },
          { "cmfs_j",    c.cmfs_j    },
          { "powr_j",    c.powr_j    },
          { "colr_j",    c.colr_j    },
          { "surface",   c.surface   }};
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

  void from_json(const json &js, IndirectSurfaceConstraint &c) {
    met_trace();
    js.at("is_base_active").get_to(c.is_base_active);
    js.at("colr_i").get_to(c.colr_i);
    js.at("target_direct").get_to(c.target_direct);
    js.at("cstr_j_direct").get_to(c.cstr_j_direct);
    js.at("cstr_j_indrct").get_to(c.cstr_j_indrct);
  }

  void to_json(json &js, const IndirectSurfaceConstraint &c) {
    met_trace();
    js = {{ "is_base_active", c.is_base_active },
          { "colr_i",         c.colr_i         },
          { "target_direct",  c.target_direct  },
          { "cstr_j_direct",  c.cstr_j_direct  },
          { "cstr_j_indrct",  c.cstr_j_indrct  }};
  }

  SpectrumSample MeasurementConstraint::realize(const Scene &scene, const Uplifting &uplifting) const { 
    auto basis = scene.resources.bases[uplifting.basis_i].value();
    SpectrumCoeffsInfo spec_info = { .spec = measure, .basis = basis };
    return generate_spectrum(spec_info); // fit basis to reproduce measure
    // return { measure, Basis::vec_type(0) }; 
  }

  SpectrumSample DirectColorConstraint::realize(const Scene &scene, const Uplifting &uplifting) const {
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

  SpectrumSample DirectSurfaceConstraint::realize(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();

    // Return zero constraint for black
    guard(!colr_i.isZero(), { Spec(0), Basis::vec_type(0) });

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
        .direct_constraints   = {{ scene.csys(uplifting.csys_i), surface.diffuse }},
        .indirect_constraints = {{ csys, colr }},
        .basis                = basis
      };

      // Generate a metamer satisfying the system+signal constraint set and return as pair
      return generate_spectrum(spec_info);
    } else { */ 
      // We attempt to fill in a default spectrum, which is necessary to establish the initial system
      // Gather all relevant color system spectra and corresponding color signals
      
      auto basis = scene.resources.bases[uplifting.basis_i].value();
      DirectSpectrumInfo spec_info = {
        .direct_constraints = {{ scene.csys(uplifting.csys_i), colr_i }},
        .basis              = basis
      };

      // Generate a metamer satisfying the system+signal constraint set and return as pair
      return generate_spectrum(spec_info);
    // }
  }

  std::vector<MismatchSample> DirectColorConstraint::realize_mismatch(const Scene &scene, const Uplifting &uplifting, uint seed, uint samples) const {
    met_trace();
    
    // Filter out inactive constraints
    auto direct_cstr = cstr_j | vws::filter(&ColrConstraint::is_active) | rng::to<std::vector>();

    // Assemble info object for generating boundary spectra
    DirectMismatchingOCSInfo info = {
      .basis     = scene.resources.bases[uplifting.basis_i].value(),
      .seed      = seed,
      .n_samples = samples
    };

    // Base roundtrip objective
    if (is_base_active)
      info.direct_objectives.push_back(scene.csys(uplifting.csys_i));

    // Specify direct color systems forming objective
    rng::transform(direct_cstr, 
      std::back_inserter(info.direct_objectives),
      [&](const auto &c) { return scene.csys(c.cmfs_j, c.illm_j); });

    // Base roundtrip constraint
    if (is_base_active)
      info.direct_constraints.push_back({ scene.csys(uplifting.csys_i), colr_i });

    // Specify direct color constraints; all but the last constraint (the "free variable") are specified
    rng::transform(direct_cstr | vws::take(direct_cstr.size() - 1), 
      std::back_inserter(info.direct_constraints),
      [&](const auto &c) { return std::pair { scene.csys(c.cmfs_j, c.illm_j), c.colr_j }; });

    return generate_mismatching_ocs(info);
  }

  std::vector<MismatchSample> DirectSurfaceConstraint::realize_mismatch(const Scene &scene, const Uplifting &uplifting, uint seed, uint samples) const {
    met_trace();
    
    // Filter out inactive constraints
    auto direct_cstr = cstr_j | vws::filter(&ColrConstraint::is_active) | rng::to<std::vector>();

    // Assemble info object for generating boundary spectra
    DirectMismatchingOCSInfo info = {
      .basis     = scene.resources.bases[uplifting.basis_i].value(),
      .seed      = seed,
      .n_samples = samples
    };

    // Base roundtrip objective
    if (is_base_active)
      info.direct_objectives.push_back(scene.csys(uplifting.csys_i));

    // Specify direct color systems forming objective
    rng::transform(direct_cstr, 
      std::back_inserter(info.direct_objectives),
      [&](const auto &c) { return scene.csys(c.cmfs_j, c.illm_j); });

    // Base roundtrip constraint
    if (is_base_active)
      info.direct_constraints.push_back({ scene.csys(uplifting.csys_i), colr_i });

    // Specify direct color constraints; all but the last constraint (the "free variable") are specified
    rng::transform(direct_cstr | vws::take(direct_cstr.size() - 1), 
      std::back_inserter(info.direct_constraints),
      [&](const auto &c) { return std::pair { scene.csys(c.cmfs_j, c.illm_j), c.colr_j }; });

    return generate_mismatching_ocs(info);
  }

  std::vector<MismatchSample> IndirectSurfaceConstraint::realize_mismatch(const Scene &scene, const Uplifting &uplifting, uint seed, uint samples) const {
    met_trace();

    // Filter out inactive constraints
    auto direct_cstr = cstr_j_direct | vws::filter(&ColrConstraint::is_active) | rng::to<std::vector>();
    auto indrct_cstr = cstr_j_indrct | vws::filter(&PowrConstraint::is_active) | rng::to<std::vector>();

    // Assemble info object for generating boundary spectra
    IndirectMismatchingOCSInfo info = {
      .basis     = scene.resources.bases[uplifting.basis_i].value(),
      .seed      = seed,
      .n_samples = samples
    };

    // Base roundtrip objective
    if (is_base_active)
      info.direct_objectives.push_back(scene.csys(uplifting.csys_i));

    // Specify direct/indirect color systems forming objective
    if (target_direct) {
      const auto &last = direct_cstr.back();
      rng::transform(direct_cstr,
        std::back_inserter(info.direct_objectives),
        [&](const auto &c) { return scene.csys(c.cmfs_j, c.illm_j); });
    } else {
      rng::transform(indrct_cstr, 
        std::back_inserter(info.indirect_objectives),
        [&](const auto &c) { return IndirectColrSystem { .cmfs = scene.resources.observers[c.cmfs_j].value(), .powers = c.powr_j }; });
    }

    // Base roundtrip constraint
    if (is_base_active)
      info.direct_constraints.push_back({ scene.csys(uplifting.csys_i), colr_i });
      
    // Specify direct/indirect color constraints; all but the last constraint (the "free variable") are specified
    if (target_direct) {
      rng::transform(direct_cstr | vws::take(direct_cstr.size() - 1), 
        std::back_inserter(info.direct_constraints),
        [&](const auto &c) { return std::pair { scene.csys(c.cmfs_j, c.illm_j), c.colr_j }; });
      rng::transform(indrct_cstr, 
      std::back_inserter(info.indirect_constraints),
        [&](const auto &c) { return std::pair { IndirectColrSystem { scene.resources.observers[c.cmfs_j].value(), c.powr_j }, c.colr_j }; });
    } else {
      rng::transform(direct_cstr, 
        std::back_inserter(info.direct_constraints),
        [&](const auto &c) { return std::pair { scene.csys(c.cmfs_j, c.illm_j), c.colr_j }; });
      rng::transform(indrct_cstr | vws::take(indrct_cstr.size() - 1), 
        std::back_inserter(info.indirect_constraints),
        [&](const auto &c) { return std::pair { IndirectColrSystem { scene.resources.observers[c.cmfs_j].value(), c.powr_j }, c.colr_j }; });
    } 

    // Output color values
    return generate_mismatching_ocs(info);
  }
} // namespace met