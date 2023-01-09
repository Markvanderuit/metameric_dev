#include <metameric/components/pipeline/task_gen_spectral_mappings.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/pca.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/data.hpp>
#include <metameric/core/state.hpp>
#include <small_gl/buffer.hpp>
#include <ranges>

namespace met {
  constexpr uint nr_maps = 16u;
  constexpr auto buffer_flags = gl::BufferCreateFlags::eStorageDynamic;

  GenSpectralMappingsTask::GenSpectralMappingsTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GenSpectralMappingsTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Specify a default buffer that can hold a default nr. of mappings
    m_max_maps = nr_maps;
    info.emplace_resource<gl::Buffer>("mapp_buffer", { .size = m_max_maps * sizeof(ColrSystem), .flags = buffer_flags });
  }
  
  void GenSpectralMappingsTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();
    
    // Continue only on relevant state change
    auto &e_pipe_state = info.get_resource<ProjectState>("state", "pipeline_state");
    guard(e_pipe_state.any_mapps);

    // Get shared resources
    auto &e_appl_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data = e_appl_data.project_data;
    auto &i_buffer    = info.get_resource<gl::Buffer>("mapp_buffer");
    
    if (e_proj_data.color_systems.size() > m_max_maps) {
      // If the maximum allowed nr. of mappings is exceeded, re-allocate with room to spare
      m_max_maps = e_proj_data.color_systems.size() + nr_maps;
      i_buffer = {{ .size = m_max_maps * sizeof(ColrSystem), .flags = buffer_flags }};
    }
    
    // Update specific, stale mapping data
    for (uint i = 0; i < e_proj_data.color_systems.size(); ++i) {
      guard_continue(e_pipe_state.mapps[i]);
      ColrSystem data = e_proj_data.csys(i);
      i_buffer.set(obj_span<const std::byte>(data), sizeof(ColrSystem), i * sizeof(ColrSystem));
    }
  }
} // namespace met