#include <metameric/core/scene.hpp>
#include <metameric/components/pipeline_new/task_gen_object_data.hpp>
#include <small_gl/texture.hpp>
#include <format>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  GenObjectDataTask:: GenObjectDataTask(uint object_i)
  : m_object_i(object_i),
    m_atlas_layer_i(0) { }

  bool GenObjectDataTask::is_active(SchedulerHandle &info) {
    met_trace();
    
    // Get external resources
    const auto &e_scene     = info.global("scene").getr<Scene>();
    const auto &e_object    = e_scene.components.objects[m_object_i];
    const auto &e_uplifting = e_scene.components.upliftings[e_object.value.uplifting_i];
    const auto &e_settings  = e_scene.components.settings;
    
    // Force on first run, then make dependent on uplifting/object/texture settings. Yikes
    return is_first_eval()               ||
           e_object.state.diffuse        ||  
           e_object.state.mesh_i         ||  
           e_object.state.uplifting_i    || // Note; we ignore object transforms
           e_uplifting.state             ||
           e_scene.resources.meshes      || 
           e_scene.resources.images      ||
           e_settings.state.texture_size ;
  }

  void GenObjectDataTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Initialize program objects in cache
    auto &e_cache = info.global("cache").getw<gl::ProgramCache>();
    std::tie(m_cache_key_txtr, std::ignore) = e_cache.set(
     {{ .type       = gl::ShaderType::eVertex,
        .spirv_path = "resources/shaders/pipeline_new/gen_tesselation_weights.vert.spv",
        .cross_path = "resources/shaders/pipeline_new/gen_tesselation_weights.vert.json",
        .spec_const = {{ 0, true }} },
      { .type       = gl::ShaderType::eFragment,
        .spirv_path = "resources/shaders/pipeline_new/gen_tesselation_weights.frag.spv",
        .cross_path = "resources/shaders/pipeline_new/gen_tesselation_weights.frag.json",
        .spec_const = {{ 0, true }} }});
    std::tie(m_cache_key_colr, std::ignore) = e_cache.set(
     {{ .type       = gl::ShaderType::eVertex,
        .spirv_path = "resources/shaders/pipeline_new/gen_tesselation_weights.vert.spv",
        .cross_path = "resources/shaders/pipeline_new/gen_tesselation_weights.vert.json",
        .spec_const = {{ 0, false }} },
      { .type       = gl::ShaderType::eFragment,
        .spirv_path = "resources/shaders/pipeline_new/gen_tesselation_weights.frag.spv",
        .cross_path = "resources/shaders/pipeline_new/gen_tesselation_weights.frag.json",
        .spec_const = {{ 0, false }} }});
                                
    // Initialize uniform buffer and writeable, flushable mapping
    m_unif_buffer = {{ .size = sizeof(UnifLayout), .flags = buffer_create_flags }};
    m_unif_map    = m_unif_buffer.map_as<UnifLayout>(buffer_access_flags).data();
    m_unif_map->object_i = m_object_i;
    m_unif_buffer.flush();
  }

  void GenObjectDataTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
    // Get external resources
    auto       &e_cache     = info.global("cache").getw<gl::ProgramCache>();
    const auto &e_scene     = info.global("scene").getr<Scene>();
    const auto &e_object    = e_scene.components.objects[m_object_i].value;
    const auto &e_uplifting = e_scene.components.upliftings[e_object.uplifting_i];

    // Target texture here is a gl-side texture atlas
    const auto &e_barycentrics = e_scene.components.upliftings.gl.texture_barycentrics;

    // Rebuild framebuffer if necessary
    if (is_first_eval() || e_barycentrics.is_invalitated() || m_atlas_layer_i != e_barycentrics.patch(m_object_i).layer_i) {
      m_atlas_layer_i = e_barycentrics.patch(m_object_i).layer_i;
      m_fbo = {{ .type       = gl::FramebufferType::eColor,
                 .attachment = &e_barycentrics.texture(),
                 .layer      = m_atlas_layer_i }};
    } 
    
    // Get external resources from object's corresponding, selected uplifting
    // An object has only one uplifting structure associated to it,
    // but several objects can reuse the same uplifting
    auto uplifting_task_name       = std::format("gen_upliftings.gen_uplifting_{}", e_object.uplifting_i);
    const auto &e_tesselation_data = info(uplifting_task_name, "tesselation_data").getr<gl::Buffer>();
    const auto &e_tesselation_pack = info(uplifting_task_name, "tesselation_pack").getr<gl::Buffer>();

    // Get ref. to relevant program for texture or color handling
    auto &program = std::holds_alternative<uint>(e_object.diffuse)
                  ? e_cache.at(m_cache_key_txtr) : e_cache.at(m_cache_key_colr);

    // Bind required resources to corresponding targets
    // Prepare program state
    program.bind();
    program.bind("b_buff_unif",        m_unif_buffer);
    program.bind("b_buff_uplift_data", e_tesselation_data);
    program.bind("b_buff_uplift_pack", e_tesselation_pack);
    program.bind("b_buff_objects",     e_scene.components.objects.gl.object_info);
    program.bind("b_buff_atlas",       e_barycentrics.buffer());
    if (std::holds_alternative<uint>(e_object.diffuse) && !e_scene.resources.images.empty()) {
      program.bind("b_txtr_3f",        e_scene.resources.images.gl.texture_atlas_3f.texture());
      program.bind("b_buff_textures",  e_scene.resources.images.gl.texture_info);
    }

    // Specify draw state
    gl::state::set_viewport(e_barycentrics.texture().size().head<2>());

    // Prepare framebuffer; no clear, just overwrite
    m_fbo.bind();

    // Get relevant draw command and dispatch draw call
    gl::sync::memory_barrier(gl::BarrierFlags::eFramebuffer        | 
                             gl::BarrierFlags::eTextureFetch       |
                             gl::BarrierFlags::eClientMappedBuffer |
                             gl::BarrierFlags::eStorageBuffer      | 
                             gl::BarrierFlags::eUniformBuffer      );
    auto command = e_scene.resources.meshes.gl.draw_commands[e_object.mesh_i];

    gl::state::set_line_width(2.f);
    gl::dispatch_draw({
      .type           = gl::PrimitiveType::eTriangles,
      .vertex_count   = command.vertex_count,
      .vertex_first   = command.vertex_first,
      .capabilities   = {{ gl::DrawCapability::eDepthTest, false },
                         { gl::DrawCapability::eCullOp,    false },
                         { gl::DrawCapability::eBlendOp,   false }},
      .draw_op        = gl::DrawOp::eLine,
      .bindable_array = &e_scene.resources.meshes.gl.array
    });
    
    gl::dispatch_draw({
      .type           = gl::PrimitiveType::eTriangles,
      .vertex_count   = command.vertex_count,
      .vertex_first   = command.vertex_first,
      .capabilities   = {{ gl::DrawCapability::eDepthTest, false },
                         { gl::DrawCapability::eCullOp,    false },
                         { gl::DrawCapability::eBlendOp,   false }},
      .draw_op        = gl::DrawOp::eFill,
      .bindable_array = &e_scene.resources.meshes.gl.array
    });
  }
} // namespace met