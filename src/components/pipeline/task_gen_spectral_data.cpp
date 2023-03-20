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

    // Setup buffer data and corresponding maps
    gl::Buffer vert_buffer = {{ .size = buffer_init_size * sizeof(eig::Array4f), .flags = buffer_create_flags}};
    gl::Buffer tetr_buffer = {{ .size = buffer_init_size * sizeof(eig::Array4f), .flags = buffer_create_flags}};
    m_vert_map = vert_buffer.map_as<eig::AlArray3f>(buffer_access_flags);
    m_tetr_map = tetr_buffer.map_as<eig::Array4u>(buffer_access_flags);

    // Submit shared resources 
    info.insert_resource<std::vector<Spec>>("vert_spec",               { }); // CPU-side generated reflectance spectra for each vertex
    info.insert_resource<AlignedDelaunayData>("delaunay",              { }); // Generated delaunay tetrahedralization over input vertices
    info.insert_resource<gl::Buffer>("vert_buffer", std::move(vert_buffer)); // OpenGL buffer storing delaunay vertex positions
    info.insert_resource<gl::Buffer>("tetr_buffer", std::move(tetr_buffer)); // OpenGL buffer storing (aligned) delaunay tetrahedral elements for compute
  }
  
  void GenSpectralDataTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Continue only on relevant state change
    auto &e_pipe_state = info.get_resource<ProjectState>("state", "pipeline_state");
    guard(e_pipe_state.any_verts);
    
    // Get shared resources
    auto &e_appl_data   = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data   = e_appl_data.project_data;
    auto &i_spectra     = info.get_resource<std::vector<Spec>>("vert_spec");
    auto &i_delaunay    = info.get_resource<AlignedDelaunayData>("delaunay");
    auto &i_vert_buffer = info.get_resource<gl::Buffer>("vert_buffer");
    auto &i_tetr_buffer = info.get_resource<gl::Buffer>("tetr_buffer");

    // Generate new delaunay structure
    std::vector<Colr> delaunay_input(e_proj_data.vertices.size());
    std::ranges::transform(e_proj_data.vertices, delaunay_input.begin(), [](const auto &vt) { return vt.colr_i; });
    i_delaunay = generate_delaunay<AlignedDelaunayData, Colr>(delaunay_input);

    // TODO: Optimize data push
    // Push new delaunay data to buffers
    std::ranges::copy(i_delaunay.verts, m_vert_map.begin());
    std::ranges::copy(i_delaunay.elems, m_tetr_map.begin());
    i_vert_buffer.flush(i_delaunay.verts.size() * sizeof(eig::AlArray3f));
    i_tetr_buffer.flush(i_delaunay.elems.size() * sizeof(eig::Array4u));

    // Generate spectra at stale gamut vertices in parallel
    i_spectra.resize(e_proj_data.vertices.size()); // vector-resize is non-destructive on vector growth
    #pragma omp parallel for
    for (int i = 0; i < i_spectra.size(); ++i) {
      // Ensure that we only continue if gamut is in any way stale
      guard_continue(e_pipe_state.verts[i].any);

      // Relevant vertex data
      auto &vert = e_proj_data.vertices[i];   

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

    // TODO: Remove dead code if irrelevant
    /* // Describe ranges over stale gamut vertices/elements
    auto vert_range = std::views::iota(0u, static_cast<uint>(e_pipe_state.verts.size()))
                    | std::views::filter([&](uint i) -> bool { return e_pipe_state.verts[i].any; });
    auto elem_range = std::views::iota(0u, static_cast<uint>(e_pipe_state.elems.size()))
                    | std::views::filter([&](uint i) -> bool { return e_pipe_state.elems[i]; });

    // Push stale gamut vertex data to gpu
    for (uint i : vert_range) {
      m_vert_map[i] = e_proj_data.vertices[i].colr_i;
      m_spec_map[i] = i_specs[i];
      i_vert_buffer.flush(sizeof(AlColr), i * sizeof(AlColr));
      i_spec_buffer.flush(sizeof(Spec), i * sizeof(Spec));
    }
    
    // Push stale gamut element data to gpu
    for (uint i : elem_range) {
      m_elem_map[i] = e_proj_data.gamut_elems[i];
      i_elem_buffer.flush(sizeof(eig::AlArray3u), i * sizeof(eig::AlArray3u));
    } */
  }
} // namespace met
