#pragma once

#include <metameric/core/fwd.hpp>

namespace met {
  // Argument struct and method for generating a spectral reflectance, given one or more
  // known color signals in corresponding color systems
  struct DirectSpectrumInfo {
    using LinearConstraint = std::pair<ColrSystem, Colr>;

  public:
    std::vector<LinearConstraint> linear_constraints = { }; // Direct metamerism constraints
    const Basis &basis;                                     // Spectral basis functions
  };
  Basis::vec_type generate_spectrum_coeffs(const DirectSpectrumInfo &info);

  // Argument struct and method for generating a spectral reflectance, given a system of
  // interreflections expressed as a truncated power series
  struct IndirectSpectrumInfo {
    using LinearConstraint  = std::pair<ColrSystem, Colr>;
    using NLinearConstraint = std::pair<IndirectColrSystem, Colr>;

  public:
    std::vector<LinearConstraint>  linear_constraints  = { }; // Direct metamerism constraints
    std::vector<NLinearConstraint> nlinear_constraints = { }; // Indirect metamerism constraints
    const Basis &basis;                                       // Spectral basis functions
  };
  Basis::vec_type generate_spectrum_coeffs(const IndirectSpectrumInfo &info);

  // Argument struct and method for generating points on the object color solid of a metameric
  // mismatching between two or more color systems, following the method of Mackiewicz et al., 2019 
  // "Spherical sampling methods for the calculation of metamer mismatch volumes"
  struct DirectMismatchingOCSInfo {
    using LinearConstraint = std::pair<ColrSystem, Colr>;

  public:
    std::vector<ColrSystem>       linear_objectives  = { }; // Direct objective functions
    std::vector<LinearConstraint> linear_constraints = { }; // Direct metamerism constraints

    const Basis &basis;  // Spectral basis functions
    uint seed      = 4;  // Seed for (pcg) sampler state
    uint n_samples = 32; // Nr. of samples to solve for
  };
  std::vector<Basis::vec_type> generate_mismatching_ocs_coeffs(const DirectMismatchingOCSInfo &info);
  
  // Argument struct and method for generating points on the object color solid of a metameric
  // mismatching between signal in a number of base color systems, and a interreflection system
  // expressed as a truncated power series
  struct IndirectMismatchingOCSInfo {
    using LinearConstraint   = std::pair<ColrSystem,         Colr>;
    using NLinearConstraint = std::pair<IndirectColrSystem, Colr>;
      
  public:
    std::vector<ColrSystem>         linear_objectives   = { }; // Direct parts of the objective function
    std::vector<IndirectColrSystem> nlinear_objectives  = { }; // Indirect parts of the objective function
    std::vector<LinearConstraint>   linear_constraints  = { }; // Direct metamerism constraints
    std::vector<NLinearConstraint>  nlinear_constraints = { }; // Indirect metamerism constraints

    const Basis &basis;  // Spectral basis functions
    uint seed      = 4;  // Seed for (pcg) sampler state
    uint n_samples = 32; // Nr. of samples to solve for
  };
  std::vector<Basis::vec_type> generate_mismatching_ocs_coeffs(const IndirectMismatchingOCSInfo &info);

  // Argument struct and method for generating points on the object color solid of a color system,
  // following the method of Mackiewicz et al., 2019 
  // "Spherical sampling methods for the calculation of metamer mismatch volumes"
  struct DirectColorSystemOCSInfo {
    ColrSystem direct_objective; // Color system that builds objective function

    const Basis &basis;  // Spectral basis functions
    uint seed      = 4;  // Seed for (pcg) sampler state
    uint n_samples = 32; // Nr. of samples to solve for
  };
  std::vector<Basis::vec_type> generate_color_system_ocs_coeffs(const DirectColorSystemOCSInfo &info);
  std::vector<Spec>            generate_color_system_ocs(const DirectColorSystemOCSInfo &info);

  // Argument struct and method for generating a closest representation in the basis
  // for a given spectral distribution.
  struct SpectrumCoeffsInfo {
    const Spec  &spec;  // Input spectrum to fit
    const Basis &basis; // Spectral basis functions
  };
  Basis::vec_type generate_spectrum_coeffs(const SpectrumCoeffsInfo &info);

  // Return type shorthands for metamer generation
  using SpectrumSample = std::pair<Spec, Basis::vec_type>;
  using MismatchSample = std::tuple<Colr, Spec, Basis::vec_type>;

  // Helpers; generate coefficients producing a spectrum in a basis, and return said spectrum
  // plus the coefficients 
  SpectrumSample generate_spectrum(const auto &info) {
    met_trace();
    auto c = generate_spectrum_coeffs(info);
    return { info.basis(c), c };
  }

  // Helpers; generate coefficients, the resulting spectrum, and the mismatched color, assuming
  // the last constraint is a "free variable"
  std::vector<MismatchSample> generate_mismatching_ocs(const DirectMismatchingOCSInfo &info);
  std::vector<MismatchSample> generate_mismatching_ocs(const IndirectMismatchingOCSInfo &info);

  /* // Helper struct to recover spectra by "rolling window" mismatch volume generation. The resulting
  // convex structure is then used to construct interior spectra through linear interpolation. 
  // This is much faster than solving for metamers directly, if the user is going to edit constraints.
  struct MismatchingOCSBuilder {
    ConvexHull chull; // Convex hull data is exposed for UI components to use
    
  private:
    using cnstr_type  = typename Uplifting::Vertex::cnstr_type;

    bool                        m_did_sample   = false;                   // Cache; did we generate samples this iteration?
    std::deque<Colr>            m_colr_samples = { };                     // For tracking incoming and exiting samples' positions
    std::deque<Basis::vec_type> m_coef_samples = { };                     // For tracking incoming and exiting samples' coefficeints
    uint                        m_curr_samples = 0;                       // How many samples are of the current vertex constraint
    uint                        m_prev_samples = 0;                       // How many samples are of an old vertex constriant
    cnstr_type                  m_cstr_cache   = DirectColorConstraint(); // Cache of current vertex constraint, to detect mismatch volume change
  }; */
} // namespace met