#include <metameric/core/scene_handler.hpp>
#include <metameric/components/views/mesh_viewport/task_draw.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/misc/detail/scene.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>

namespace met {
  bool MeshViewportDrawTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info.relative("view_begin")("is_active").read_only<bool>()
       && !info.global("scene_handler").read_only<SceneHandler>().scene.components.objects.empty();
  }

  void MeshViewportDrawTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Initialize program object
    m_program = {{ .type       = gl::ShaderType::eVertex,
                    .spirv_path = "resources/shaders/views/draw_mesh.vert.spv",
                    .cross_path = "resources/shaders/views/draw_mesh.vert.json" },
                  { .type       = gl::ShaderType::eFragment,
                    .spirv_path = "resources/shaders/views/draw_mesh.frag.spv",
                    .cross_path = "resources/shaders/views/draw_mesh.frag.json" }};

    // Initialize uniform buffer and corresponding mappings
    constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
    constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
    m_unif_buffer     = {{ .size = sizeof(UnifLayout), .flags = buffer_create_flags }};
    m_unif_buffer_map = m_unif_buffer.map_as<UnifLayout>(buffer_access_flags).data();

    // Initialize draw object
    m_draw = { 
      .type             = gl::PrimitiveType::eTriangles,
      .capabilities     = {{ gl::DrawCapability::eMSAA,      true },
                            { gl::DrawCapability::eDepthTest, true },
                            { gl::DrawCapability::eCullOp,    true }},
      .draw_op          = gl::DrawOp::eFill,
      .bindable_program = &m_program 
    };
  }
    
  void MeshViewportDrawTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources 
    const auto &e_scene    = info.global("scene_handler").read_only<SceneHandler>().scene;
    const auto &e_arcball  = info.relative("view_input")("arcball").read_only<detail::Arcball>();
    const auto &e_meshes   = info("scene_handler", "meshes").read_only<std::vector<detail::MeshLayout>>();
    const auto &e_textures = info("scene_handler", "textures").read_only<std::vector<detail::TextureLayout>>();

    // Bind required fixed resources to corresponding targets
    m_program.bind("b_unif", m_unif_buffer);

    // Push camera matrix to uniform data
    m_unif_buffer_map->camera_matrix = e_arcball.full().matrix();

    // Iterate over components in the scene and draw them
    for (const auto &component : e_scene.components.objects) {
      // Skip components flagged as inactive
      guard_continue(component.is_active);

      // Gather relevant component and resource data
      const auto &object   = component.value;
      const auto &material = e_scene.components.materials[object.material_i].value;
      const auto &mesh     = e_scene.resources.meshes[object.mesh_i].value();

      // Test if mesh data is available yet on the GL side
      guard_continue(e_meshes.size() > object.mesh_i);

      // Push model matrix to uniform data
      m_unif_buffer_map->model_matrix = object.trf.matrix();

      // Bind relevant diffuse texture data if exists, or specify color else
      if (material.diffuse.index() == 1) { // texture
        // Test if texture data is available yet on the GL side
        uint texture_i = std::get<1>(material.diffuse);
        guard_continue(e_textures.size() > texture_i);

        // Bind texture and sampler to corresponding targets
        const auto &layout = e_textures[texture_i];
        m_unif_buffer_map->use_diffuse_texture = true;
        m_program.bind("b_diffuse_texture", *layout.texture);
        m_program.bind("b_diffuse_texture",  layout.sampler);
      } else { // constant value
        m_unif_buffer_map->use_diffuse_texture = false;
        m_unif_buffer_map->diffuse_value       = std::get<0>(material.diffuse);
      }

      // Push uniform data
      m_unif_buffer.flush();

      // Adjust draw object for coming draw
      m_draw.vertex_count   = static_cast<uint>(mesh.elems.size()) * 3;
      m_draw.bindable_array = &e_meshes[object.mesh_i].array;

      // Dispatch draw call
      gl::dispatch_draw(m_draw);
    } // for (component)
  }
} // namespace met