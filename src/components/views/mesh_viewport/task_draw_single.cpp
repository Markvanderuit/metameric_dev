#include <metameric/core/scene_handler.hpp>
#include <metameric/components/views/mesh_viewport/task_draw_single.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/misc/detail/scene.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  bool MeshViewportSingleDrawTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info.relative("view_begin")("is_active").read_only<bool>()
       && !info.global("scene_handler").read_only<SceneHandler>().scene.components.objects.empty();
  }

  void MeshViewportSingleDrawTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Initialize program object
    m_program = {{ .type       = gl::ShaderType::eVertex,
                    .spirv_path = "resources/shaders/views/draw_mesh_single.vert.spv",
                    .cross_path = "resources/shaders/views/draw_mesh_single.vert.json" },
                  { .type       = gl::ShaderType::eFragment,
                    .spirv_path = "resources/shaders/views/draw_mesh_single.frag.spv",
                    .cross_path = "resources/shaders/views/draw_mesh_single.frag.json" }};

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
    
  void MeshViewportSingleDrawTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources 
    const auto &e_scene    = info.global("scene_handler").read_only<SceneHandler>().scene;
    const auto &e_arcball  = info.relative("view_input")("arcball").read_only<detail::Arcball>();
    const auto &e_meshes   = info("scene_handler", "meshes").read_only<std::vector<detail::MeshLayout>>();
    const auto &e_textures = info("scene_handler", "textures").read_only<std::vector<detail::TextureLayout>>();
    const auto &e_mesh_data = info("scene_handler", "mesh_data").read_only<detail::RTMeshData>();
    const auto &e_objc_data = info("scene_handler", "objc_data").read_only<detail::RTObjectData>();

    // Nr. of object components
    uint n = e_scene.components.objects.size();

    // Push camera matrix to uniform data
    m_unif_camera_buffer_map->camera_matrix = e_arcball.full().matrix();
    m_unif_camera_buffer.flush();

    // Bind required fixed resources to corresponding targets
    m_program.bind("b_unif_camera",  m_unif_camera_buffer);
    m_program.bind("b_buff_objects", e_objc_data.info_gl);
    
    // Assemble draw appropriate commands for each object in the scene
    m_draw.bindable_array = &e_mesh_data.array;
    m_draw.commands.clear();
    rng::transform(e_scene.components.objects, std::back_inserter(m_draw.commands), [&](const auto &component) {
      const auto &e_mesh_info = e_mesh_data.info.at(component.value.mesh_i);
      return gl::MultiDrawInfo::DrawCommand {
        .vertex_count   = e_mesh_info.elems_size * 3,
        .vertex_first   = e_mesh_info.elems_offs * 3,
        .instance_count = 1
      };
    });

    // Dispatch draw call to handle entire scene
    gl::dispatch_multidraw(m_draw);

    /* // Iterate over components in the scene and draw them
    for (uint i = 0; i < n; ++i) {
      // Gather relevant component and resource data
      const auto &component = e_scene.components.objects[i];
      const auto &object   = component.value;
      const auto &mesh     = e_scene.resources.meshes[object.mesh_i].value();

      // Skip object if flagged as inactive
      guard_continue(object.is_active);

      // Skip component if mesh data is not yet pushed on the GL side
      guard_continue(e_meshes.size() > object.mesh_i);

      // Get object uniform mapping
      auto &buffer     = m_unif_object_buffers[i];
      auto &buffer_map = m_unif_object_buffer_maps[i];

      // Bind relevant diffuse texture data if exists, or specify color else
      if (object.diffuse.index() == 1) { // texture
        // Test if texture data is available yet on the GL side
        uint texture_i = std::get<1>(object.diffuse);
        guard_continue(e_textures.size() > texture_i);

        // Bind texture and sampler to corresponding targets
        const auto &layout = e_textures[texture_i];
        m_program.bind("b_diffuse_texture",  layout.sampler);
        m_program.bind("b_diffuse_texture", *layout.texture);
      } else { // constant value
        buffer_map->diffuse_value = std::get<0>(object.diffuse);
      }
      
      // Push object uniform data
      buffer_map->model_matrix        = object.trf.matrix();
      buffer_map->use_diffuse_texture = object.diffuse.index();
      buffer.flush();

      // Adjust draw object for coming draw
      m_draw.vertex_count   = static_cast<uint>(mesh.elems.size()) * 3;
      m_draw.bindable_array = &e_meshes[object.mesh_i].array;

      m_program.bind("b_unif_object", buffer);

      // Dispatch draw call
      gl::dispatch_draw(m_draw);
    } // for (uint i) */
  }
} // namespace met