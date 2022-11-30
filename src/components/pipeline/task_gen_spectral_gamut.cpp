#include <metameric/components/pipeline/task_gen_spectral_gamut.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/knn.hpp>
#include <metameric/core/linprog.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/pca.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/detail/trace.hpp>
#include <small_gl/buffer.hpp>
#include <algorithm>
#include <execution>
#include <ranges>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite 
                                     | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite 
                                     | gl::BufferAccessFlags::eMapPersistent
                                     | gl::BufferAccessFlags::eMapFlush;

  constexpr std::array<uint, 16> gamut_elems_al = {
    2, 0, 1, 0, // 4 padding bytes
    0, 3, 1, 0, // 4 padding bytes 
    2, 1, 3, 0, // 4 padding bytes
    0, 2, 3, 0  // 4 padding bytes
  };

  GenSpectralGamutTask::GenSpectralGamutTask(const std::string &name)
  : detail::AbstractTask(name) { }
  
  void GenSpectralGamutTask::init(detail::TaskInitInfo &info) {
    met_trace_full();
    
    // Define color and spectral gamut buffers
    gl::Buffer buffer_elem = {{ .data = cnt_span<const std::byte>(gamut_elems_al) }};

    // Submit shared resources 
    info.insert_resource("buffer_elem",  std::move(buffer_elem));
    info.insert_resource<std::vector<Spec>>("gamut_spec", { });
    info.insert_resource<gl::Buffer>("buffer_colr", { });
    info.insert_resource<gl::Buffer>("buffer_spec", { });
    info.insert_resource<std::span<AlColr>>("mapping_colr", { });
    info.insert_resource<std::span<Spec>>("mapping_spec", { });
  }

  void GenSpectralGamutTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();

    // Unmap buffers
    info.get_resource<gl::Buffer>("buffer_colr").unmap();
    info.get_resource<gl::Buffer>("buffer_spec").unmap();
  }
  
  void GenSpectralGamutTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Continue only on relevant state change
    auto &e_state_gamut  = info.get_resource<std::vector<CacheState>>("project_state", "gamut_summary");
    guard(std::ranges::any_of(e_state_gamut, [](auto s) { return s == CacheState::eStale; }));

    // Get shared resources
    auto &i_gamut_spec   = info.get_resource<std::vector<Spec>>("gamut_spec");
    auto &i_buffer_colr  = info.get_resource<gl::Buffer>("buffer_colr");
    auto &i_mapping_colr = info.get_resource<std::span<AlColr>>("mapping_colr");
    auto &i_buffer_spec  = info.get_resource<gl::Buffer>("buffer_spec");
    auto &i_mapping_spec = info.get_resource<std::span<Spec>>("mapping_spec");
    auto &e_basis        = info.get_resource<BMatrixType>(global_key, "pca_basis");
    auto &e_app_data     = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data    = e_app_data.project_data;

    // Resize gamut vector and associated buffers in case nr. of vertices changes
    if (e_state_gamut.size() != i_gamut_spec.size()) {
      i_gamut_spec.resize(e_state_gamut.size());

      if (i_buffer_spec.is_init() && i_buffer_spec.is_mapped()) i_buffer_spec.unmap();
      if (i_buffer_colr.is_init() && i_buffer_colr.is_mapped()) i_buffer_colr.unmap();
      
      i_buffer_spec = {{ .size  = e_state_gamut.size() * sizeof(Spec),   .flags = buffer_create_flags }};
      i_buffer_colr = {{ .size  = e_state_gamut.size() * sizeof(AlColr), .flags = buffer_create_flags }};
      
      i_mapping_spec = cast_span<Spec>(i_buffer_spec.map(buffer_access_flags));
      i_mapping_colr = cast_span<AlColr>(i_buffer_colr.map(buffer_access_flags));
    }

    // Generate spectra at gamut color positions
    #pragma omp parallel for
    for (int i = 0; i < i_gamut_spec.size(); ++i) {
      // Ensure that we only continue if gamut is in any way stale
      guard_continue(e_state_gamut[i] == CacheState::eStale);
      
      // Generate new metameric spectrum for given color systems and expected color signals
      std::array<CMFS, 2> systems = { e_app_data.loaded_mappings[e_proj_data.gamut_mapp_i[i]].finalize(i_gamut_spec[i]),
                                      e_app_data.loaded_mappings[e_proj_data.gamut_mapp_j[i]].finalize(i_gamut_spec[i]) };
      std::array<Colr, 2> signals = { e_proj_data.gamut_colr_i[i], 
                                     (e_proj_data.gamut_colr_i[i] + e_proj_data.gamut_offs_j[i]).eval() };
      
      // Generate new spectrum given the above systems+signals as solver constraints
      i_gamut_spec[i] = generate(e_basis.rightCols(wavelength_bases), systems, signals);
    }

    // Re-upload stale gamut data to the gpu
    for (uint i = 0; i < e_proj_data.gamut_colr_i.size(); ++i) {
      guard_continue(e_state_gamut[i] == CacheState::eStale);

      i_mapping_colr[i] = e_proj_data.gamut_colr_i[i];
      i_mapping_spec[i] = i_gamut_spec[i];
      i_buffer_colr.flush(sizeof(AlColr), i * sizeof(AlColr));
      i_buffer_spec.flush(sizeof(Spec), i * sizeof(Spec));
    }
  }
} // namespace met
