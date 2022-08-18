#include <metameric/components/tasks/task_gen_ocs.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/utility.hpp>
#include <numbers>

namespace met {
  constexpr uint n_samples = 4096;
  
  GeOCSTask::GeOCSTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GeOCSTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Initialize objects for shader call
    m_program = {{ .type = gl::ShaderType::eCompute,
                   .path = "resources/shaders/gen_ocs/gen_ocs.comp" }};
    m_dispatch = { .groups_x = ceil_div(n_samples, 256u), 
                   .bindable_program = &m_program };

    // Set these uniforms once
    m_program.uniform("u_n", n_samples);
    m_program.uniform("u_mapping_i", 0u);

    // Initialize main buffers
    info.emplace_resource<gl::Buffer>("color_buffer", {
      .size = static_cast<size_t>(n_samples) * sizeof(eig::AlArray3f),
      .flags = gl::BufferCreateFlags::eMapFull
    });
    info.emplace_resource<gl::Buffer>("spectrum_buffer", {
      .size = static_cast<size_t>(n_samples) * sizeof(Spec),
      .flags = gl::BufferCreateFlags::eMapFull
    });

    m_stale = true;
  }

  void GeOCSTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Execute once, for now
    guard(m_stale); 
    m_stale = false;
    
    // Get shared resources
    auto &e_app_data    = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_mappings    = e_app_data.loaded_mappings;
    auto &i_colr_buffer = info.get_resource<gl::Buffer>("color_buffer");
    auto &i_spec_buffer = info.get_resource<gl::Buffer>("spectrum_buffer");
    auto &e_mapp_buffer = info.get_resource<gl::Buffer>("gen_spectral_mappings", "mappings_buffer");

    // Open maps
    auto colr_map = i_colr_buffer.map_as<eig::AlArray3f>(gl::BufferAccessFlags::eMapReadWrite);
    auto spec_map = i_spec_buffer.map_as<Spec>(gl::BufferAccessFlags::eMapReadWrite);

    // Obtain color system spectra
    CMFS cs = e_mappings[0].finalize();

    // Generate samples
    for (uint i = 0; i < n_samples; ++i) {
      // Generate random unit vector as point on sphere
      auto sv = eig::Array2f::Random() * 0.5f + 0.5f;
      float t = 2.f * std::numbers::pi_v<float> * sv.x();
      float o = std::acosf(1.f - 2.f * sv.y());
      auto uv = eig::Vector3f(std::sinf(o) * std::cosf(t),
                              std::sinf(o) * std::sinf(t),
                              std::cosf(o));

      // Generate the algorithm's matrix A_ij and the optimal spectrum R_ij and a mapped color
      Spec a_ij = uv.transpose() * cs.matrix().transpose();
      Spec r_ij = (a_ij >= 0.f).select(Spec(1.f), Spec(0.f));
      auto c_ij = cs.matrix().transpose() * r_ij.matrix();

      // Write results to buffers
      spec_map[i] = r_ij;
      colr_map[i] = c_ij;
    }

    // Close maps and flush data
    i_colr_buffer.unmap();
    i_spec_buffer.unmap();

    /* // Bind buffer resources to ssbo targets
    e_mapp_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    i_spec_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    i_colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 2);
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);

    // Dispatch shader to sample optimal spectra on OCS border
    gl::dispatch_compute(m_dispatch);
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::sync::memory_barrier(gl::BarrierFlags::eBufferUpdate); */

    // Allocate buffers and transfer data for output
    std::vector<eig::AlArray3f> colr_buffer(n_samples);
    std::vector<Spec>           spec_buffer(n_samples);
    i_colr_buffer.get_as<eig::AlArray3f>(colr_buffer);
    i_spec_buffer.get_as<Spec>(spec_buffer);
    for (uint i = 0; i < 256; ++i) {
      fmt::print("{} -> {}\n", spec_buffer[i], colr_buffer[i]);
    }
  }
}