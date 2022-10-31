#include <metameric/components/pipeline/task_gen_spectral_gamut.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/knn.hpp>
#include <metameric/core/linprog.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/pca.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <small_gl/buffer.hpp>
#include <algorithm>
#include <execution>
#include <ranges>

namespace met {
  GenSpectralGamutTask::GenSpectralGamutTask(const std::string &name)
  : detail::AbstractTask(name) { }
  
  void GenSpectralGamutTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Define flags for creation of a persistent, write-only flushable buffer map
    auto create_flags = gl::BufferCreateFlags::eMapWrite 
                      | gl::BufferCreateFlags::eMapPersistent;
    auto map_flags    = gl::BufferAccessFlags::eMapWrite 
                      | gl::BufferAccessFlags::eMapPersistent
                      | gl::BufferAccessFlags::eMapFlush;
    
    // Define color and spectral gamut buffers
    gl::Buffer buffer_colr = {{ .size  = 4 * sizeof(AlColr), .flags = create_flags }};
    gl::Buffer buffer_spec = {{ .size  = 4 * sizeof(Spec), .flags = create_flags }};

    // Prepare buffer maps which are written to often
    auto buffer_colr_map = cast_span<AlColr>(buffer_colr.map(map_flags));
    auto buffer_spec_map = cast_span<Spec>(buffer_spec.map(map_flags));

    // Submit resources 
    info.insert_resource("buffer_colr",     std::move(buffer_colr));
    info.insert_resource("buffer_spec",     std::move(buffer_spec));
    info.insert_resource("buffer_colr_map", std::move(buffer_colr_map));
    info.insert_resource("buffer_spec_map", std::move(buffer_spec_map));
  }

  void GenSpectralGamutTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &i_buffer_colr = info.get_resource<gl::Buffer>("buffer_colr");
    auto &i_buffer_spec = info.get_resource<gl::Buffer>("buffer_spec");

    // Unmap buffers
    i_buffer_colr.unmap();
    i_buffer_spec.unmap();
  }
  
  void GenSpectralGamutTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_app_data        = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &i_buffer_colr     = info.get_resource<gl::Buffer>("buffer_colr");
    auto &i_buffer_spec     = info.get_resource<gl::Buffer>("buffer_spec");
    auto &i_buffer_colr_map = info.get_resource<std::span<AlColr>>("buffer_colr_map");
    auto &i_buffer_spec_map = info.get_resource<std::span<Spec>>("buffer_spec_map");
    auto &e_basis           = info.get_resource<BMatrixType>(global_key, "pca_basis");

    // Get relevant application/project data shorthands
    auto &e_proj_data  = e_app_data.project_data;
    auto &e_mappings   = e_app_data.loaded_mappings;

    // Get relevant shared state data
    auto &e_state_mappings = info.get_resource<std::vector<CacheState>>("project_state", "mappings");
    auto &e_state_gamut    = info.get_resource<std::array<CacheState, 4>>("project_state", "gamut_summary");
    auto &e_state_spec     = info.get_resource<std::array<CacheState, 4>>("project_state", "gamut_spec");

    // Generate spectra at gamut color positions
    // #pragma omp parallel for
    for (int i = 0; i < e_proj_data.gamut_colr_i.size(); ++i) {
      // Ensure that we only continue if gamut data or mapping data is in any way stale
      const uint mapp_i = e_proj_data.gamut_mapp_i[i], mapp_j = e_proj_data.gamut_mapp_j[i];
      guard_continue(e_state_gamut[i]         == CacheState::eStale 
                  || e_state_mappings[mapp_i] == CacheState::eStale 
                  || e_state_mappings[mapp_j] == CacheState::eStale);

      std::array<CMFS, 2> systems = { e_mappings[e_proj_data.gamut_mapp_i[i]].finalize(e_proj_data.gamut_spec[i]),
                                      e_mappings[e_proj_data.gamut_mapp_j[i]].finalize(e_proj_data.gamut_spec[i]) };
      std::array<Colr, 2> signals = { e_proj_data.gamut_colr_i[i], 
                                     (e_proj_data.gamut_colr_i[i] + e_proj_data.gamut_offs_j[i]).eval() };
      
      // Generate new metameric spectrum for given color systems and expected color signals
      e_proj_data.gamut_spec[i] = generate(e_basis.rightCols(wavelength_bases), systems, signals);
    }

    // Re-upload stale gamut data to the gpu
    for (uint i = 0; i < e_state_spec.size(); ++i) {
      guard_continue(e_state_spec[i] == CacheState::eStale || e_state_gamut[i] == CacheState::eStale);
      fmt::print("Uploaded stale gamut {}\n", i);
      i_buffer_colr_map[i] = e_proj_data.gamut_colr_i[i];
      i_buffer_spec_map[i] = e_proj_data.gamut_spec[i];
      i_buffer_colr.flush(sizeof(AlColr), i * sizeof(AlColr));
      i_buffer_spec.flush(sizeof(Spec), i * sizeof(Spec));
    }
  }
} // namespace met
