#include <metameric/core/math.hpp>
#include <metameric/core/linprog.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/data.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/pipeline/task_gen_spectral_data.hpp>
#include <small_gl/buffer.hpp>
#include <algorithm>
#include <execution>
#include <ranges>
#include <unordered_set>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;
  constexpr uint buffer_init_size    = 1024u;

  void GenSpectralDataTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Submit shared resources 
    info("vert_spec").set<std::vector<Spec>>({ }); // CPU-side generated reflectance spectra for each vertex
  }

  bool GenSpectralDataTask::is_active(SchedulerHandle &info) {
    met_trace_full();
    return info("state", "pipeline_state").read_only<ProjectState>().any_verts;
  }
  
  void GenSpectralDataTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
    // Get external resources
    const auto &e_pipe_state = info("state", "pipeline_state").read_only<ProjectState>();
    const auto &e_appl_data  = info.global("app_data").read_only<ApplicationData>();
    const auto &e_proj_data  = e_appl_data.project_data;

    // Get modified resources
    auto &i_spectra = info("vert_spec").writeable<std::vector<Spec>>();

    // Generate spectra at stale gamut vertices in parallel
    i_spectra.resize(e_proj_data.verts.size()); // vector-resize is non-destructive on vector growth
    #pragma omp parallel for
    for (int i = 0; i < i_spectra.size(); ++i) {
      // Ensure that we only continue if gamut is in any way stale
      guard_continue(e_pipe_state.verts[i].any);

      // Relevant vertex data
      auto &vert = e_proj_data.verts[i];   

      // Obtain color system spectra for this vertex
      std::vector<CMFS> systems = { e_proj_data.csys(vert.csys_i).finalize_indirect(i_spectra[i]) };
      std::ranges::transform(vert.csys_j, std::back_inserter(systems), [&](uint j) { return e_proj_data.csys(j).finalize_direct(); });

      // Obtain corresponding color signal for each color system
      std::vector<Colr> signals(1 + vert.colr_j.size());
      signals[0] = vert.colr_i;
      std::ranges::copy(vert.colr_j, signals.begin() + 1);

      // Generate new spectrum given the above systems+signals as solver constraints
      i_spectra[i] = generate_spectrum({ 
        .basis      = e_appl_data.loaded_basis,
        .basis_mean = e_appl_data.loaded_basis_mean,
        .systems    = std::span<CMFS> { systems }, 
        .signals    = std::span<Colr> { signals },
        .reduce_basis_count = false
      });
    }
  }
} // namespace met
