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

  GenSpectralGamutTask::GenSpectralGamutTask(const std::string &name)
  : detail::AbstractTask(name) { }
  
  void GenSpectralGamutTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Submit shared resources 
    info.insert_resource<std::vector<Spec>>("gamut_spec", { });
    info.insert_resource<gl::Buffer>("colr_buffer", { });
    info.insert_resource<gl::Buffer>("spec_buffer", { });
    info.insert_resource<gl::Buffer>("elem_buffer", { });
    info.insert_resource<gl::Buffer>("elem_buffer_unal", { });
  }

  void GenSpectralGamutTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &i_spec_buffer = info.get_resource<gl::Buffer>("spec_buffer");
    auto &i_colr_buffer = info.get_resource<gl::Buffer>("colr_buffer");
    auto &i_elem_buffer = info.get_resource<gl::Buffer>("elem_buffer");
    auto &i_elem_buffer_= info.get_resource<gl::Buffer>("elem_buffer_unal");

    // Unmap buffers
    if (i_spec_buffer.is_init() && i_spec_buffer.is_mapped()) i_spec_buffer.unmap();
    if (i_colr_buffer.is_init() && i_colr_buffer.is_mapped()) i_colr_buffer.unmap();
    if (i_elem_buffer.is_init() && i_elem_buffer.is_mapped()) i_elem_buffer.unmap();
    if (i_elem_buffer_.is_init() && i_elem_buffer_.is_mapped()) i_elem_buffer_.unmap();
  }
  
  void GenSpectralGamutTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Continue only on relevant state change
    auto &e_state_gamut  = info.get_resource<std::vector<CacheState>>("project_state", "gamut_summary");
    guard(std::ranges::any_of(e_state_gamut, [](auto s) { return s == CacheState::eStale; }));

    // Get shared resources
    auto &e_state_elems  = info.get_resource<std::vector<CacheState>>("project_state", "gamut_elems");
    auto &i_gamut_spec   = info.get_resource<std::vector<Spec>>("gamut_spec");
    auto &i_colr_buffer  = info.get_resource<gl::Buffer>("colr_buffer");
    auto &i_spec_buffer  = info.get_resource<gl::Buffer>("spec_buffer");
    auto &i_elem_buffer  = info.get_resource<gl::Buffer>("elem_buffer");
    auto &i_elem_buffer_ = info.get_resource<gl::Buffer>("elem_buffer_unal");
    auto &e_basis        = info.get_resource<BMatrixType>(global_key, "pca_basis");
    auto &e_app_data     = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data    = e_app_data.project_data;
    auto &e_gamut_colr_i = e_proj_data.gamut_colr_i;
    auto &e_gamut_elems  = e_proj_data.gamut_elems;

    // Resize spectrum data vector and re-create gamut buffers in case of nr. of vertices changes
    if (e_gamut_colr_i.size() != i_gamut_spec.size()) {
      i_gamut_spec.resize(e_gamut_colr_i.size());

      if (i_spec_buffer.is_init() && i_spec_buffer.is_mapped()) i_spec_buffer.unmap();
      if (i_colr_buffer.is_init() && i_colr_buffer.is_mapped()) i_colr_buffer.unmap();
      if (i_elem_buffer.is_init() && i_elem_buffer.is_mapped()) i_elem_buffer.unmap();
      if (i_elem_buffer_.is_init() && i_elem_buffer_.is_mapped()) i_elem_buffer_.unmap();
      
      i_spec_buffer = {{ .size  = e_gamut_colr_i.size() * sizeof(Spec),          .flags = buffer_create_flags }};
      i_colr_buffer = {{ .size  = e_gamut_colr_i.size() * sizeof(AlColr),        .flags = buffer_create_flags }};
      i_elem_buffer = {{ .size  = e_gamut_elems.size() * sizeof(eig::AlArray3u), .flags = buffer_create_flags }};
      i_elem_buffer_= {{ .size  = e_gamut_elems.size() * sizeof(eig::Array3u),   .flags = buffer_create_flags }};
      
      m_spec_map = cast_span<Spec>(i_spec_buffer.map(buffer_access_flags));
      m_colr_map = cast_span<AlColr>(i_colr_buffer.map(buffer_access_flags));
      m_elem_map = cast_span<eig::AlArray3u>(i_elem_buffer.map(buffer_access_flags));
      m_elem_unal_map= cast_span<eig::Array3u>(i_elem_buffer_.map(buffer_access_flags));
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

    // Describe ranges over stale gamut vertex/elements
    auto vert_range = std::views::iota(0u, static_cast<uint>(e_gamut_colr_i.size()))
                    | std::views::filter([&](uint i) { return e_state_gamut[i] == CacheState::eStale; });
    auto elem_range = std::views::iota(0u, static_cast<uint>(e_state_elems.size()))
                    | std::views::filter([&](uint i) { return e_state_elems[i] == CacheState::eStale; });

    // Push stale gamut vertex data to gpu
    for (uint i : vert_range) {
      m_colr_map[i] = e_proj_data.gamut_colr_i[i];
      m_spec_map[i] = i_gamut_spec[i];
      i_colr_buffer.flush(sizeof(AlColr), i * sizeof(AlColr));
      i_spec_buffer.flush(sizeof(Spec), i * sizeof(Spec));
    }
    
    // Push stale gamut element data to gpu
    for (uint i : elem_range) {
      m_elem_map[i]      = e_proj_data.gamut_elems[i];
      m_elem_unal_map[i] = e_proj_data.gamut_elems[i];
      i_elem_buffer.flush(sizeof(eig::AlArray3u), i * sizeof(eig::AlArray3u));
      i_elem_buffer_.flush(sizeof(eig::Array3u), i * sizeof(eig::Array3u));
    }
  }
} // namespace met
