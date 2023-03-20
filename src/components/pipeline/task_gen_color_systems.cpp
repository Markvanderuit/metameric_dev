#include <metameric/components/pipeline/task_gen_color_systems.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/data.hpp>
#include <metameric/core/state.hpp>
#include <small_gl/buffer.hpp>
#include <ranges>

namespace met {
  constexpr uint default_nr_maps = 16u;
  constexpr auto buffer_flags = gl::BufferCreateFlags::eStorageDynamic;

  struct UnalColrSystem {
    UnalColrSystem(const ColrSystem &s) : cmfs(s.cmfs), illm(s.illuminant) { }

    eig::Matrix<float, wavelength_samples, 3, eig::DontAlign> cmfs;
    eig::Array<float, wavelength_samples, 1, eig::DontAlign>  illm;
  };
  
  void GenColorSystemsTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Specify a default buffer that can hold a default nr. of mappings
    m_max_maps = default_nr_maps;
    info.emplace_resource<gl::Buffer>("mapp_buffer", { .size = m_max_maps * sizeof(ColrSystem), .flags = buffer_flags });
  }
  
  void GenColorSystemsTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
    // Continue only on relevant state change
    auto &e_pipe_state = info.get_resource<ProjectState>("state", "pipeline_state");
    guard(e_pipe_state.any_csys);

    // Get shared resources
    auto &e_appl_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data = e_appl_data.project_data;
    auto &i_buffer    = info.get_resource<gl::Buffer>("mapp_buffer");
    
    if (e_proj_data.color_systems.size() > m_max_maps) {
      // If the maximum allowed nr. of mappings is exceeded, re-allocate with room to spare
      m_max_maps = e_proj_data.color_systems.size() + default_nr_maps;
      i_buffer = {{ .size = m_max_maps * sizeof(ColrSystem), .flags = buffer_flags }};
    }
    
    // Update specific, stale mapping data
    for (uint i = 0; i < e_proj_data.color_systems.size(); ++i) {
      guard_continue(e_pipe_state.csys[i]);
      auto data = UnalColrSystem(e_proj_data.csys(i));
      i_buffer.set(obj_span<const std::byte>(data), sizeof(decltype(data)), i * sizeof(decltype(data)));
    }
  }
} // namespace met