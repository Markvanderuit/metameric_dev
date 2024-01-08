#include <metameric/core/scene.hpp>
#include <metameric/components/views/mesh_viewport/task_draw_combine.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/render/renderer.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/dispatch.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

 /*  bool MeshViewportDrawCombineTask::is_active(SchedulerHandle &info) {
    met_trace();
    return (info.relative("viewport_begin")("is_active").getr<bool>()
        ||  info.relative("viewport_input")("arcball").is_mutated())
       && !info.global("scene").getr<Scene>().components.objects.empty();
  } */

  void MeshViewportDrawCombineTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Initialize program object
    m_program = {{ .type       = gl::ShaderType::eCompute,
                   .spirv_path = "resources/shaders/views/draw_mesh_combine.comp.spv",
                   .cross_path = "resources/shaders/views/draw_mesh_combine.comp.json" }};

    // Initialize uniform buffers and corresponding mappings
    m_unif_buffer     = {{ .size = sizeof(UnifLayout), .flags = buffer_create_flags }};
    m_unif_buffer_map = m_unif_buffer.map_as<UnifLayout>(buffer_access_flags).data();
  }
    
  void MeshViewportDrawCombineTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources 
    const auto &e_scene  = info.global("scene").getr<Scene>();
    const auto &e_target = info.relative("viewport_begin")("lrgb_target").getr<gl::Texture2d4f>();
    const auto &e_direct = info.relative("viewport_draw_direct")("direct_renderer").getr<PathRenderer>();

    // Specify dispatch size
    auto dispatch_n    = e_target.size();
    auto dispatch_ndiv = ceil_div(dispatch_n, 16u);

    // Push miscellaneous uniforms
    m_unif_buffer_map->viewport_size = dispatch_n;
    m_unif_buffer.flush();

    // Bind required resources to their corresponding targets
    m_program.bind("b_buff_unif", m_unif_buffer);
    m_program.bind("b_direct_4f", e_direct.film());
    m_program.bind("b_target_4f", e_target);

    // Dispatch compute shader
    gl::sync::memory_barrier(gl::BarrierFlags::eImageAccess | gl::BarrierFlags::eTextureFetch);
    gl::dispatch_compute({ .groups_x         = dispatch_ndiv.x(),
                           .groups_y         = dispatch_ndiv.y(),
                           .bindable_program = &m_program       });
  }
} // namespace met