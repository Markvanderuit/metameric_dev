#include <metameric/components/pipeline/task_gen_spectral_mappings.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/pca.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <small_gl/buffer.hpp>
#include <ranges>

namespace met {
  constexpr uint nr_maps = 4u;
  constexpr auto buffer_flags = gl::BufferCreateFlags::eStorageDynamic;

  GenSpectralMappingsTask::GenSpectralMappingsTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GenSpectralMappingsTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Specify a default mappings buffer that can hold a appropriate nr. of mappings
    m_max_maps = nr_maps;
    info.emplace_resource<gl::Buffer>("mapp_buffer", { .size = m_max_maps * sizeof(Mapp), .flags = buffer_flags });
  }
  
  void GenSpectralMappingsTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_vector_mapp = info.get_resource<ApplicationData>(global_key, "app_data").loaded_mappings;
    auto &i_buffer_mapp = info.get_resource<gl::Buffer>("mapp_buffer");
    auto &e_state_mapp  = info.get_resource<std::vector<CacheState>>("project_state", "mappings");

    if (e_vector_mapp.size() > m_max_maps) {
      // If the maximum allowed nr. of mappings is exceeded, re-allocate with room to spare
      m_max_maps = e_vector_mapp.size() + nr_maps;
      i_buffer_mapp = {{ .size = m_max_maps * sizeof(Mapp), .flags = buffer_flags }};

      // Re-upload entire mapping data
      auto span = cnt_span<const std::byte>(e_vector_mapp);
      i_buffer_mapp.set(span, span.size_bytes());
    } else {
      // Re-upload only stale mapping data to the gpu
      for (uint i = 0; i < e_vector_mapp.size(); ++i) {
        guard_continue(e_state_mapp[i] == CacheState::eStale);
        i_buffer_mapp.set(obj_span<const std::byte>(e_vector_mapp[i]), sizeof(Mapp), i * sizeof(Mapp));
      }
    }
  }
} // namespace met