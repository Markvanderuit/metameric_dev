#include <metameric/core/scene_handler.hpp>
#include <metameric/components/views/mesh_viewport/task_draw.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/misc/detail/scene.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

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

    // Initialize uniform camera buffer and corresponding mapping
    m_unif_camera_buffer     = {{ .size = sizeof(UnifCameraLayout), .flags = buffer_create_flags }};
    m_unif_camera_buffer_map = m_unif_camera_buffer.map_as<UnifCameraLayout>(buffer_access_flags).data();

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

    // Nr. of object components
    uint n = e_scene.components.objects.size();

    // Reserve a specific nr. of uniform buffers
    m_unif_object_buffers.resize(n);
    m_unif_object_buffer_maps.resize(n, nullptr);
    for (uint i = 0; i < n; ++i) {
      auto &buffer     = m_unif_object_buffers[i];
      auto &buffer_map = m_unif_object_buffer_maps[i];

      guard_continue(!buffer.is_init() || !buffer_map);
      
      buffer     = {{ .size = sizeof(UnifObjectLayout), .flags = buffer_create_flags }};
      buffer_map = buffer.map_as<UnifObjectLayout>(buffer_access_flags).data();
    } // for (uint i)

    // Push camera matrix to uniform data
    m_unif_camera_buffer_map->camera_matrix = e_arcball.full().matrix();
    m_unif_camera_buffer.flush();

    // Bind required fixed resources to corresponding targets
    m_program.bind("b_unif_camera", m_unif_camera_buffer);
    
    // Iterate over components in the scene and draw them
    for (uint i = 0; i < n; ++i) {
      // Gather relevant component and resource data
      const auto &component = e_scene.components.objects[i];
      const auto &object   = component.value;
      const auto &material = e_scene.components.materials[object.material_i].value;
      const auto &mesh     = e_scene.resources.meshes[object.mesh_i].value();

      // Skip object if flagged as inactive
      guard_continue(object.is_active);

      // Skip component if mesh data is not yet pushed on the GL side
      guard_continue(e_meshes.size() > object.mesh_i);

      // Get object uniform mapping
      auto &buffer     = m_unif_object_buffers[i];
      auto &buffer_map = m_unif_object_buffer_maps[i];

      // Bind relevant diffuse texture data if exists, or specify color else
      if (material.diffuse.index() == 1) { // texture
        // Test if texture data is available yet on the GL side
        uint texture_i = std::get<1>(material.diffuse);
        guard_continue(e_textures.size() > texture_i);

        // Bind texture and sampler to corresponding targets
        const auto &layout = e_textures[texture_i];
        m_program.bind("b_diffuse_texture",  layout.sampler);
        m_program.bind("b_diffuse_texture", *layout.texture);
      } else { // constant value
        buffer_map->diffuse_value       = std::get<0>(material.diffuse);
      }
      
      // Push object uniform data
      buffer_map->model_matrix        = object.trf.matrix();
      buffer_map->use_diffuse_texture = material.diffuse.index();
      buffer.flush();

      // Adjust draw object for coming draw
      m_draw.vertex_count   = static_cast<uint>(mesh.elems.size()) * 3;
      m_draw.bindable_array = &e_meshes[object.mesh_i].array;

      m_program.bind("b_unif_object", buffer);

      // Dispatch draw call
      gl::dispatch_draw(m_draw);
    } // for (uint i)
  }
} // namespace met