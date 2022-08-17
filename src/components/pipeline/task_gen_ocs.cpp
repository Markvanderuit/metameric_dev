#include <metameric/components/tasks/task_gen_ocs.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/utility.hpp>

namespace met {
  constexpr uint n_samples = 16384;
  
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
      .size = static_cast<size_t>(n_samples) * sizeof(eig::AlArray3f)
    });
    info.emplace_resource<gl::Buffer>("spectrum_buffer", {
      .size = static_cast<size_t>(n_samples) * sizeof(Spec)
    });

    m_stale = true;
  }

  void GeOCSTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();
    // guard(m_stale); // Execute once, for now
    
    // Get shared resources
    auto &i_colr_buffer = info.get_resource<gl::Buffer>("color_buffer");
    auto &i_spec_buffer = info.get_resource<gl::Buffer>("spectrum_buffer");
    auto &e_mapp_buffer = info.get_resource<gl::Buffer>("gen_spectral_mappings", "mappings_buffer");

    // Bind buffer resources to ssbo targets
    e_mapp_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    i_spec_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    i_colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 2);
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);

    // Dispatch shader to sample optimal spectra on OCS border
    gl::dispatch_compute(m_dispatch);
    
    // m_stale = false;
  }
}