#include <metameric/components/tasks/task_gen_spectral_mappings.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/pca.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <small_gl/buffer.hpp>
#include <ranges>

namespace met {
  GenSpectralMappingsTask::GenSpectralMappingsTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GenSpectralMappingsTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_bases    = info.get_resource<SMatrix>(global_key, "pca_basis");
    auto &e_mappings = e_app_data.loaded_mappings;

    // Specify a default mappings buffer and store the nr. of mappings
    m_mapping_count = 0;
    info.insert_resource("mappings_buffer", gl::Buffer());
    
    // Generate metameric blacks for D65 for now
    BBasis basis = e_bases.rightCols(wavelength_bases);
    BCMFS  bcmfs = (e_mappings[0].finalize().transpose() * basis).transpose().eval();
    BBlack black = orthogonal_complement<wavelength_bases, 3>(bcmfs);
    MetamerMapping mmapping {
      .mapping_i   = e_mappings[0],
      .mapping_j   = e_mappings[1],
      .basis_funcs = basis,
      .black_funcs = black
    };
    info.insert_resource<MetamerMapping>("metamer_mapping", std::move(mmapping));
  }
  
  void GenSpectralMappingsTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_app_data        = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &i_mappings_buffer = info.get_resource<gl::Buffer>("mappings_buffer");

    // Check if mappings data is stale
    if (m_mapping_count != e_app_data.loaded_mappings.size()) {
      m_mapping_count = e_app_data.loaded_mappings.size();

      // (Re-)generate the stored mapping buffer
      i_mappings_buffer = {{
        .data = cnt_span<const std::byte>(e_app_data.loaded_mappings),
        .flags = gl::BufferCreateFlags::eStorageDynamic
      }};
    } else {
      // TODO this should be improved to only happen ONCE
      // Upload new data to the stored mapping buffer if stale
      i_mappings_buffer.set(cnt_span<const std::byte>(e_app_data.loaded_mappings));
    }   
  }
} // namespace met