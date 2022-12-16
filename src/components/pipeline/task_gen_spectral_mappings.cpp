#include <metameric/components/pipeline/task_gen_spectral_mappings.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/pca.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/data.hpp>
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
    info.emplace_resource<gl::Buffer>("mapp_buffer", { .size = m_max_maps * sizeof(Mapp), .flags = buffer_flags });
  }
  
  void GenSpectralMappingsTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_mappings = e_app_data.loaded_mappings;
    auto &e_state    = e_app_data.project_state;
    auto &i_buffer   = info.get_resource<gl::Buffer>("mapp_buffer");

    // Continue only on relevant state change
    guard(e_state.any_mapps);
    
    if (e_mappings.size() > m_max_maps) {
      // If the maximum allowed nr. of mappings is exceeded, re-allocate with room to spare
      m_max_maps = e_mappings.size() + nr_maps;
      i_buffer   = {{ .size = m_max_maps * sizeof(Mapp), .flags = buffer_flags }};

      // Re-upload entire mapping data in one go
      auto span = cnt_span<const std::byte>(e_mappings);
      i_buffer  .set(span, span.size_bytes());
    } else {
      // Update specific, stale mapping data
      for (uint i = 0; i < e_mappings.size(); ++i) {
        guard_continue(e_state.mapps[i]);
        i_buffer  .set(obj_span<const std::byte>(e_mappings[i]), sizeof(Mapp), i * sizeof(Mapp));
      }
    }
  }
} // namespace met