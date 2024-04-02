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

  bool IndirectConstraint::operator==(const IndirectConstraint &o) const {
    return cmfs_i == o.cmfs_i
        && illm_i == o.illm_i
        && colr_i.isApprox(o.colr_i)
        && cmfs_j == o.cmfs_j
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

  void from_json(const json &js, IndirectConstraint &c) {
    met_trace();
    js.at("cmfs_i").get_to(c.cmfs_i);
    js.at("illm_i").get_to(c.illm_i);
    js.at("colr_i").get_to(c.colr_i);
    js.at("cmfs_j").get_to(c.cmfs_j);
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
          { "cmfs_j", c.cmfs_j },
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

  std::pair<Colr, Spec> MeasurementConstraint::realize(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();
    CMFS csys = scene.csys(uplifting.csys_i).finalize();
    return { (csys.transpose() * measure.matrix()).eval(), measure };
  }

  std::pair<Colr, Spec> DirectColorConstraint::realize(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();

    // Gather all relevant color system spectra and corresponding color signals
    std::vector<CMFS> systems = { scene.csys(uplifting.csys_i).finalize() };
    std::vector<Colr> signals = { colr_i };
    for (const auto &c : cstr_j) {
      systems.push_back(scene.csys(c.cmfs_j, c.illm_j).finalize());
      signals.push_back(c.colr_j);
    }

    // Generate a metamer satisfying the system+signal constraint set
    Spec s = generate_spectrum(GenerateSpectrumInfo {
      .basis   = scene.resources.bases[uplifting.basis_i].value(),
      .systems = systems,
      .signals = signals
    });
    
    return { colr_i, s };
  }

  std::pair<Colr, Spec> DirectSurfaceConstraint::realize(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();

    // Return zero constraint for invalid surfaces
    guard(has_surface(), { Colr(0), Spec(0) });

    // Gather all relevant color system spectra and corresponding color signals
    std::vector<CMFS> systems = { scene.csys(uplifting.csys_i).finalize() };
    std::vector<Colr> signals = { colr_i };
    for (const auto &c : cstr_j) {
      systems.push_back(scene.csys(c.cmfs_j, c.illm_j).finalize());
      signals.push_back(c.colr_j);
    }

    // Generate a metamer satisfying the system+signal constraint set
    Spec s = generate_spectrum(GenerateSpectrumInfo {
      .basis   = scene.resources.bases[uplifting.basis_i].value(),
      .systems = systems,
      .signals = signals
    });
    
    return { colr_i, s };
  }

  std::pair<Colr, Spec> IndirectSurfaceConstraint::realize(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();
    
    // Return zero constraint for invalid surfaces
    guard(has_surface(), { Colr(0), Spec(0) });

    if (has_mismatching()) {
      // Generate color system spectra from power series
      IndirectColrSystem csys = {
        .cmfs   = scene.resources.observers[scene.components.observer_i.value].value(),
        .powers = powers
      };
      auto refl_systems = csys.finalize();
              
      // Generate a metamer satisfying the constraint set
      Spec s = generate_spectrum(GenerateIndirectSpectrumInfo {
        .basis        = scene.resources.bases[uplifting.basis_i].value(),
        .base_system  = scene.csys(uplifting.csys_i).finalize(),
        .base_signal  = surface.diffuse,
        .refl_systems = refl_systems,
        .refl_signal  = colr
      });

      return { surface.diffuse, s };
    } else { // We attempt to fill in a default spectrum, which is necessary to establish the initial system
      // Gather all relevant color system spectra and corresponding color signals
      std::vector<CMFS> systems = { scene.csys(uplifting.csys_i).finalize() };
      std::vector<Colr> signals = { surface.diffuse };

      // Generate a metamer satisfying the system+signal constraint set
      Spec s = generate_spectrum(GenerateSpectrumInfo {
        .basis   = scene.resources.bases[uplifting.basis_i].value(),
        .systems = systems,
        .signals = signals
      });

      return { surface.diffuse, s };
    }
  }

  std::vector<Colr> MeasurementConstraint::realize_mismatching(const Scene &scene, const Uplifting &uplifting, uint csys_i, uint seed, uint samples) const {
    met_trace();
    return { };
  }

  std::vector<Colr> DirectColorConstraint::realize_mismatching(const Scene &scene, const Uplifting &uplifting, uint csys_i, uint seed, uint samples) const {
    met_trace();

    // Prepare input color systems and corresponding signals
    auto systems_i = { scene.csys(uplifting.csys_i).finalize() };
    auto signals_i = { colr_i };
    std::vector<CMFS> systems_j;
    for (const auto &c : cstr_j)
      systems_j.push_back(scene.csys(c.cmfs_j, c.illm_j).finalize());

    // Prepare data for MMV point generation
    GenerateMismatchingOCSInfo mmv_info = {
      .basis     = scene.resources.bases[uplifting.basis_i].value(),
      .systems_i = systems_i,
      .signals_i = signals_i,
      .systems_j = systems_j,
      .seed      = seed,
      .n_samples = samples
    };

    auto csys = scene.csys(cstr_j[csys_i].cmfs_j, cstr_j[csys_i].illm_j);
    return csys(generate_mismatching_ocs(mmv_info));
  }

  std::vector<Colr> DirectSurfaceConstraint::realize_mismatching(const Scene &scene, const Uplifting &uplifting, uint csys_i, uint seed, uint samples) const {
    met_trace();

    // Prepare input color systems and corresponding signals
    auto systems_i = { scene.csys(uplifting.csys_i).finalize() };
    auto signals_i = { colr_i };
    std::vector<CMFS> systems_j;
    for (const auto &c : cstr_j)
      systems_j.push_back(scene.csys(c.cmfs_j, c.illm_j).finalize());

    // Prepare data for MMV point generation
    GenerateMismatchingOCSInfo mmv_info = {
      .basis     = scene.resources.bases[uplifting.basis_i].value(),
      .systems_i = systems_i,
      .signals_i = signals_i,
      .systems_j = systems_j,
      .seed      = seed,
      .n_samples = samples
    };

    auto csys = scene.csys(cstr_j[csys_i].cmfs_j, cstr_j[csys_i].illm_j);
    return csys(generate_mismatching_ocs(mmv_info));
  }

  std::vector<Colr> IndirectSurfaceConstraint::realize_mismatching(const Scene &scene, const Uplifting &uplifting, uint csys_i, uint seed, uint samples) const {
    met_trace();

    // Construct objective color system spectra from power series
    IndirectColrSystem csys = {
      .cmfs   = scene.resources.observers[scene.components.observer_i.value].value(),
      .powers = powers
    };

    // Prepare input color systems and corresponding signals;
    // note, we keep search space in XYZ
    auto csys_base_v = { scene.csys(uplifting.csys_i).finalize(false) };
    auto sign_base_v = { lrgb_to_xyz(surface.diffuse) };
    auto csys_refl_v = csys.finalize(false);

    // Assemble info object for generating MMV spectra
    GenerateIndirectMismatchingOCSInfo mmv_info = {
      .basis     = scene.resources.bases[uplifting.basis_i].value(),
      .systems_i = csys_base_v,
      .signals_i = sign_base_v,
      .systems_j = csys_refl_v,
      .seed      = seed,
      .n_samples = samples
    };

    return csys(generate_mismatching_ocs(mmv_info));
  }
} // namespace met