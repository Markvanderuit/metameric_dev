#include <metameric/core/scene.hpp>
#include <metameric/components/views/mesh_viewport/task_draw.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/render/scene_data.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  bool MeshViewportDrawTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info.relative("viewport_begin")("is_active").getr<bool>()
       && !info.global("scene").getr<Scene>().components.objects.empty();
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
      .bindable_program = &m_program,
    };
  }
    
  void MeshViewportDrawTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources 
    const auto &e_scene     = info.global("scene").getr<Scene>();
    const auto &e_objects   = e_scene.components.objects;
    const auto &e_arcball   = info.relative("viewport_input")("arcball").getr<detail::Arcball>();
    const auto &e_objc_data = info("scene_handler", "objc_data").getr<ObjectData>();
    const auto &e_mesh_data = info("scene_handler", "mesh_data").getr<MeshData>();
    const auto &e_txtr_data = info("scene_handler", "txtr_data").getr<TextureData>();

    // Push camera matrix to uniform data
    m_unif_camera_buffer_map->camera_matrix = e_arcball.full().matrix();
    m_unif_camera_buffer.flush();
    
    // Set fresh vertex array for draw data if it was updated
    if (is_first_eval() || info("scene_handler", "mesh_data").is_mutated())
      m_draw.bindable_array = &e_mesh_data.array;

    // Assemble appropriate draw data for each object in the scene
    if (is_first_eval() || info("scene_handler", "objc_data").is_mutated()) {
      m_draw.commands.resize(e_objects.size());
      rng::transform(e_objects, m_draw.commands.begin(), [&](const auto &comp) {
        guard(comp.value.is_active, gl::MultiDrawInfo::DrawCommand { });
        const auto &e_mesh_info = e_mesh_data.info.at(comp.value.mesh_i);
        return gl::MultiDrawInfo::DrawCommand {
          .vertex_count = e_mesh_info.elems_size * 3,
          .vertex_first = e_mesh_info.elems_offs * 3
        };
      });
    }

    // Bind required resources to their corresponding targets
    m_program.bind("b_unif_camera",   m_unif_camera_buffer);
    m_program.bind("b_buff_objects",  e_objc_data.info_gl);
    if (e_txtr_data.info_gl.is_init())
      m_program.bind("b_buff_textures", e_txtr_data.info_gl);
    if (e_txtr_data.atlas_1f.texture().is_init())
      m_program.bind("b_txtr_1f", e_txtr_data.atlas_1f.texture());
    if (e_txtr_data.atlas_3f.texture().is_init())
      m_program.bind("b_txtr_3f", e_txtr_data.atlas_3f.texture());

    // Dispatch draw call to handle entire scene
    gl::dispatch_multidraw(m_draw);
  }
} // namespace met