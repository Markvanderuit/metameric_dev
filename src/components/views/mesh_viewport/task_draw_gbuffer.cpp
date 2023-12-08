#include <metameric/core/scene.hpp>
#include <metameric/components/views/mesh_viewport/task_draw_gbuffer.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/misc/detail/scene.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  bool MeshViewportDrawGBufferTask::is_active(SchedulerHandle &info) {
    met_trace();

    auto is_objc_present = !info.global("scene").getr<Scene>().components.objects.empty();
    auto is_view_present = info.relative("viewport_begin")("is_active").getr<bool>();
    auto is_objc_updated = info("scene_handler", "objc_data").is_mutated();
    auto is_view_updated = info.relative("viewport_begin")("lrgb_target").is_mutated()
                       ||  info.relative("viewport_input")("arcball").is_mutated();

    return is_objc_present && is_view_present && (is_objc_updated || is_view_updated);
  }

  void MeshViewportDrawGBufferTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Initialize program object
    m_program = {{ .type       = gl::ShaderType::eVertex,
                   .spirv_path = "resources/shaders/views/draw_mesh_gbuffer.vert.spv",
                   .cross_path = "resources/shaders/views/draw_mesh_gbuffer.vert.json" },
                 { .type       = gl::ShaderType::eFragment,
                   .spirv_path = "resources/shaders/views/draw_mesh_gbuffer.frag.spv",
                   .cross_path = "resources/shaders/views/draw_mesh_gbuffer.frag.json" }};

    // Initialize uniform camera buffer and corresponding mapping
    m_unif_buffer     = {{ .size = sizeof(UnifLayout), .flags = buffer_create_flags }};
    m_unif_buffer_map = m_unif_buffer.map_as<UnifLayout>(buffer_access_flags).data();

    // Initialize draw object
    m_draw = { 
      .type             = gl::PrimitiveType::eTriangles,
      .capabilities     = {{ gl::DrawCapability::eDepthTest, true },
                           { gl::DrawCapability::eCullOp,    true }},
      .draw_op          = gl::DrawOp::eFill,
      .bindable_program = &m_program,
    };

    info("gbuffer").set<gl::Texture2d4f>({ }); // packed gbuffer texture
  }
    
  void MeshViewportDrawGBufferTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get handle to relative task resource
    auto target_handle  = info.relative("viewport_begin")("lrgb_target");
    auto arcball_handle = info.relative("viewport_input")("arcball");

    // Get shared resources 
    const auto &e_scene     = info.global("scene").getr<Scene>();
    const auto &e_objects   = e_scene.components.objects;
    const auto &e_arcball   = info.relative("viewport_input")("arcball").getr<detail::Arcball>();
    const auto &e_objc_data = info("scene_handler", "objc_data").getr<detail::RTObjectData>();
    const auto &e_mesh_data = info("scene_handler", "mesh_data").getr<detail::RTMeshData>();
    const auto &e_target    = target_handle.getr<gl::Texture2d4f>();

    // TODO: this violates EVERYTHING in terms of state, but test it
    // if (!target_handle.is_mutated() && !arcball_handle.is_mutated())
    //   return;

    // Rebuild framebuffer and g-buffer textures if necessary
    if (!m_fbo.is_init() || (m_fbo_depth.size() != e_target.size()).any()) {
      // Get write-handles to g-buffer textures
      auto &i_gbuffer = info("gbuffer").getw<gl::Texture2d4f>();
      
      // Rebuild depth target and g-buffer textures
      m_fbo_depth = {{ .size = e_target.size().max(1).eval() }};
      i_gbuffer   = {{ .size = e_target.size().max(1).eval() }};

      // Rebuild framebuffer
      m_fbo = {{ .type = gl::FramebufferType::eColor, .attachment = &i_gbuffer   },
               { .type = gl::FramebufferType::eDepth, .attachment = &m_fbo_depth }};
    }

    // Push camera matrix to uniform data
    if (target_handle.is_mutated() || arcball_handle.is_mutated()) {
      m_unif_buffer_map->trf = e_arcball.full().matrix();
      m_unif_buffer.flush();
    }
    
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

    // Prepare framebuffer state
    m_fbo.bind();
    m_fbo.clear(gl::FramebufferType::eColor, eig::Array4f(0), 0);
    m_fbo.clear(gl::FramebufferType::eDepth, 1.f);
    
    // Bind required resources to their corresponding targets
    m_program.bind("b_buff_unif",    m_unif_buffer);
    m_program.bind("b_buff_objects", e_objc_data.info_gl);
    m_program.bind("b_buff_meshes",  e_mesh_data.info_gl);

    // Dispatch draw call to handle entire scene
    gl::sync::memory_barrier(gl::BarrierFlags::eFramebuffer        | 
                             gl::BarrierFlags::eTextureFetch       |
                             gl::BarrierFlags::eClientMappedBuffer |
                             gl::BarrierFlags::eUniformBuffer      );
    gl::dispatch_multidraw(m_draw);
  }
} // namespace met