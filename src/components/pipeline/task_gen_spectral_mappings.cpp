#include <metameric/components/tasks/task_gen_spectral_mappings.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <small_gl/buffer.hpp>
#include <ranges>

namespace met {
  GenSpectralMappingsTask::GenSpectralMappingsTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GenSpectralMappingsTask::init(detail::TaskInitInfo &info) {
    // Specify a default mappings buffer and store the nr. of mappings
    m_mapping_count = 0;
    info.insert_resource("mappings_buffer", gl::Buffer());
  }
  
  void GenSpectralMappingsTask::eval(detail::TaskEvalInfo &info) {
    // Get shared resources
    auto &e_app_data        = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &i_mappings_buffer = info.get_resource<gl::Buffer>("mappings_buffer");

    // Check if mappings data is stale
    if (m_mapping_count != e_app_data.loaded_mappings.size()) {
      m_mapping_count = e_app_data.loaded_mappings.size();

      // (Re-)generate the stored mapping buffer
      i_mappings_buffer = {{
        .data = as_span<const std::byte>(e_app_data.loaded_mappings),
        .flags = gl::BufferCreateFlags::eStorageDynamic
      }};
    } else {
      // TODO this should be improved to only happen ONCE
      // Upload new data to the stored mapping buffer if stale
      i_mappings_buffer.set(as_span<const std::byte>(e_app_data.loaded_mappings));
    }

    
  }
} // namespace met