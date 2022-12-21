#include <metameric/core/math.hpp>
#include <metameric/core/linprog.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/pca.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/data.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/pipeline/task_gen_spectral_gamut.hpp>
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
    info.insert_resource<gl::Buffer>("vert_buffer",       { });
    info.insert_resource<gl::Buffer>("spec_buffer",       { });
    info.insert_resource<gl::Buffer>("elem_buffer",       { });
    info.insert_resource<gl::Buffer>("elem_buffer_unal",  { });
  }

  void GenSpectralGamutTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &i_spec_buffer = info.get_resource<gl::Buffer>("spec_buffer");
    auto &i_vert_buffer = info.get_resource<gl::Buffer>("vert_buffer");
    auto &i_elem_buffer = info.get_resource<gl::Buffer>("elem_buffer");
    auto &i_elem_buffer_= info.get_resource<gl::Buffer>("elem_buffer_unal");

    // Unmap buffers
    if (i_spec_buffer.is_init() && i_spec_buffer.is_mapped()) i_spec_buffer.unmap();
    if (i_vert_buffer.is_init() && i_vert_buffer.is_mapped()) i_vert_buffer.unmap();
    if (i_elem_buffer.is_init() && i_elem_buffer.is_mapped()) i_elem_buffer.unmap();
    if (i_elem_buffer_.is_init() && i_elem_buffer_.is_mapped()) i_elem_buffer_.unmap();
  }
  
  void GenSpectralGamutTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Continue only on relevant state change
    auto &e_appl_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_pipe_state = info.get_resource<ProjectState>("state", "pipeline_state");
    guard(e_pipe_state.any_verts);
    
    // Get shared resources
    auto &e_proj_data   = e_appl_data.project_data;
    auto &e_elems       = e_proj_data.gamut_elems;
    auto &e_verts       = e_proj_data.gamut_verts;
    auto &e_basis       = info.get_resource<BMatrixType>(global_key, "pca_basis");
    auto &i_specs       = info.get_resource<std::vector<Spec>>("gamut_spec");
    auto &i_vert_buffer = info.get_resource<gl::Buffer>("vert_buffer");
    auto &i_elem_buffer = info.get_resource<gl::Buffer>("elem_buffer");
    auto &i_elem_buffer_= info.get_resource<gl::Buffer>("elem_buffer_unal");
    auto &i_spec_buffer = info.get_resource<gl::Buffer>("spec_buffer");

    // Resize spectrum data and re-create buffers if nr. of vertices changes
    if (e_verts.size() != i_specs.size()) {
      i_specs.resize(e_verts.size());

      if (i_spec_buffer.is_init() && i_spec_buffer.is_mapped())   i_spec_buffer.unmap();
      if (i_vert_buffer.is_init() && i_vert_buffer.is_mapped())   i_vert_buffer.unmap();
      if (i_elem_buffer.is_init() && i_elem_buffer.is_mapped())   i_elem_buffer.unmap();
      if (i_elem_buffer_.is_init() && i_elem_buffer_.is_mapped()) i_elem_buffer_.unmap();
      
      i_spec_buffer = {{ .size  = e_verts.size() * sizeof(Spec),           .flags = buffer_create_flags }};
      i_vert_buffer = {{ .size  = e_verts.size() * sizeof(AlColr),         .flags = buffer_create_flags }};
      i_elem_buffer = {{ .size  = e_elems.size() * sizeof(eig::AlArray3u), .flags = buffer_create_flags }};
      i_elem_buffer_= {{ .size  = e_elems.size() * sizeof(eig::Array3u),   .flags = buffer_create_flags }};
      
      m_spec_map = cast_span<Spec>(i_spec_buffer.map(buffer_access_flags));
      m_vert_map = cast_span<AlColr>(i_vert_buffer.map(buffer_access_flags));
      m_elem_map = cast_span<eig::AlArray3u>(i_elem_buffer.map(buffer_access_flags));
      m_elem_unal_map = cast_span<eig::Array3u>(i_elem_buffer_.map(buffer_access_flags));
    }

    // Generate spectra at gamut color positions in parallel
    #pragma omp parallel for
    for (int i = 0; i < i_specs.size(); ++i) {
      // Ensure that we only continue if gamut is in any way stale
      guard_continue(e_pipe_state.verts[i].any);

      // Relevant vertex data
      auto &vert = e_proj_data.gamut_verts[i];   

      // Obtain color system spectra for this vertex
      std::vector<CMFS> systems = { e_proj_data.mapping_data(vert.mapp_i).finalize() };
      std::ranges::transform(vert.mapp_j, std::back_inserter(systems), 
        [&](uint j) { return e_proj_data.mapping_data(j).finalize(); });

      // Obtain corresponding color signal for each color system
      std::vector<Colr> signals(1 + vert.colr_j.size());
      signals[0] = vert.colr_i;
      std::ranges::copy(vert.colr_j, signals.begin() + 1);

      // Generate new spectrum given the above systems+signals as solver constraints
      i_specs[i] = generate(e_basis.rightCols(wavelength_bases), systems, signals);
    }

    // Describe ranges over stale gamut vertices/elements
    auto vert_range = std::views::iota(0u, static_cast<uint>(e_pipe_state.verts.size()))
                    | std::views::filter([&](uint i) -> bool { return e_pipe_state.verts[i].any; });
    auto elem_range = std::views::iota(0u, static_cast<uint>(e_pipe_state.elems.size()))
                    | std::views::filter([&](uint i) -> bool { return e_pipe_state.elems[i]; });

    // Push stale gamut vertex data to gpu
    for (uint i : vert_range) {
      m_vert_map[i] = e_proj_data.gamut_verts[i].colr_i;
      m_spec_map[i] = i_specs[i];
      i_vert_buffer.flush(sizeof(AlColr), i * sizeof(AlColr));
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
