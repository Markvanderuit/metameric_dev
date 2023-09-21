#include <metameric/core/data.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/components/pipeline/task_gen_spectral_data.hpp>
#include <algorithm>
#include <execution>
#include <ranges>
#include <unordered_set>

namespace met {
  void GenSpectralDataTask::init(SchedulerHandle &info) {
    met_trace();

    // Submit shared resources 
    info("spectra").set<std::vector<Spec>>({ }); // CPU-side generated reflectance spectra for each vertex
  }

  bool GenSpectralDataTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info("state", "proj_state").read_only<ProjectState>().verts;
  }
  
  void GenSpectralDataTask::eval(SchedulerHandle &info) {
    met_trace();
    
    // Get external resources
    const auto &e_proj_state = info("state", "proj_state").read_only<ProjectState>();
    const auto &e_appl_data  = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data  = e_appl_data.project_data;

    // Get modified resources
    auto &i_spectra = info("spectra").writeable<std::vector<Spec>>();

    // Resize is non-destructive on growth, and otherwise data needs deleting anyways
    i_spectra.resize(e_proj_data.verts.size()); 

    // Generate spectra at stale gamut vertices in parallel
    #pragma omp parallel for
    for (int i = 0; i < i_spectra.size(); ++i) {
      // We only generate a spectrum if the specific vertex is stale
      guard_continue(e_proj_state.verts[i]);
      const auto &vert = e_proj_data.verts[i];   

      // Obtain color system spectra for this vertex
      std::vector<CMFS> systems = { e_proj_data.csys(vert.csys_i).finalize_indirect(i_spectra[i]) };
      std::ranges::transform(vert.csys_j, std::back_inserter(systems), [&](uint j) { return e_proj_data.csys(j).finalize_direct(); });

      // Obtain corresponding color signal for each color system
      std::vector<Colr> signals = { vert.colr_i };
      std::ranges::copy(vert.colr_j, std::back_inserter(signals));

      // Generate new spectrum given the above systems+signals as solver constraints
      i_spectra[i] = generate_spectrum({ 
        .basis      = e_appl_data.loaded_basis,
        .systems    = std::span<CMFS> { systems }, 
        .signals    = std::span<Colr> { signals },
        .solve_dual = true
      });
    }
  }
} // namespace met
