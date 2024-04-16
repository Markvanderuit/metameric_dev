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
    const auto &e_scene        = info.global("scene").getr<Scene>();
    const auto &e_object       = e_scene.components.objects[m_object_i];
    const auto &e_uplifting    = e_scene.components.upliftings[e_object.value.uplifting_i];
    const auto &e_settings     = e_scene.components.settings;
    const auto &e_barycentrics = e_scene.components.upliftings.gl.texture_barycentrics;

    // Force on first run, then make dependent on uplifting/object/texture settings. Yikes
    return is_first_eval()                 || // First run demands render
           e_barycentrics.is_invalitated() || // Texture atlas re-allocate demands rerender
           e_object.state.diffuse          || // Note; we ignore object transforms
           e_object.state.mesh_i           ||  
           e_object.state.uplifting_i      || 
           e_uplifting.state               ||
           e_scene.resources.meshes        || 
           e_scene.resources.images        ||
           e_settings.state.texture_size   ;
  }

  void GenObjectDataTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Initialize program objects in cache
    auto &e_cache = info.global("cache").getw<gl::ProgramCache>();
    std::tie(m_cache_key_txtr, std::ignore) = e_cache.set(
     {{ .type       = gl::ShaderType::eVertex,
        .spirv_path = "resources/shaders/pipeline_new/gen_texture.vert.spv",
        .cross_path = "resources/shaders/pipeline_new/gen_texture.vert.json" },
      { .type       = gl::ShaderType::eGeometry,
        .spirv_path = "resources/shaders/pipeline_new/gen_texture.geom.spv",
        .cross_path = "resources/shaders/pipeline_new/gen_texture.geom.json" },
      { .type       = gl::ShaderType::eFragment,
        .spirv_path = "resources/shaders/pipeline_new/gen_texture.frag.spv",
        .cross_path = "resources/shaders/pipeline_new/gen_texture.frag.json",
        .spec_const = {{ 0, true }} }});
    std::tie(m_cache_key_colr, std::ignore) = e_cache.set(
     {{ .type       = gl::ShaderType::eVertex,
        .spirv_path = "resources/shaders/pipeline_new/gen_texture.vert.spv",
        .cross_path = "resources/shaders/pipeline_new/gen_texture.vert.json" },
      { .type       = gl::ShaderType::eGeometry,
        .spirv_path = "resources/shaders/pipeline_new/gen_texture.geom.spv",
        .cross_path = "resources/shaders/pipeline_new/gen_texture.geom.json" },
      { .type       = gl::ShaderType::eFragment,
        .spirv_path = "resources/shaders/pipeline_new/gen_texture.frag.spv",
        .cross_path = "resources/shaders/pipeline_new/gen_texture.frag.json",
        .spec_const = {{ 0, false }} }});
    std::tie(m_cache_key_bake, std::ignore) = e_cache.set(
     {{ .type       = gl::ShaderType::eCompute,
        .spirv_path = "resources/shaders/pipeline_new/gen_moments.comp.spv",
        .cross_path = "resources/shaders/pipeline_new/gen_moments.comp.json" }});
                                
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

    // Find relevant patch in texture atlas
    const auto &e_barycentrics = e_scene.components.upliftings.gl.texture_barycentrics;
    const auto &e_coefficients = e_scene.components.upliftings.gl.texture_coefficients;
    const auto &e_patch        = e_barycentrics.patch(m_object_i);

    // Rebuild framebuffer if necessary
    if (is_first_eval() || e_barycentrics.is_invalitated() || m_atlas_layer_i != e_patch.layer_i) {
      m_atlas_layer_i = e_patch.layer_i;
      m_fbo = {{ .type       = gl::FramebufferType::eColor,
                 .index      = 0,
                 .attachment = &e_barycentrics.texture(),
                 .layer      = m_atlas_layer_i },
               { .type       = gl::FramebufferType::eColor,
                 .index      = 1,
                 .attachment = &e_coefficients.texture(),
                 .layer      = m_atlas_layer_i }};
    } 
    
    // Get external resources from object's corresponding, selected uplifting
    // An object has only one uplifting structure associated to it,
    // but several objects can reuse the same uplifting
    auto uplifting_task_name       = std::format("gen_upliftings.gen_uplifting_{}", e_object.uplifting_i);
    const auto &e_tesselation_data = info(uplifting_task_name, "tesselation_data").getr<gl::Buffer>();
    const auto &e_tesselation_pack = info(uplifting_task_name, "tesselation_pack").getr<gl::Buffer>();
    const auto &e_tesselation_coef = info(uplifting_task_name, "tesselation_coef").getr<gl::Buffer>();

    { // First dispatch; determine barycentric weights and index per pixel
      
      // Get ref. to relevant program for texture or color handling,
      // then bind relevant resources to corresponding targets
      auto &program = std::holds_alternative<uint>(e_object.diffuse)
                    ? e_cache.at(m_cache_key_txtr) : e_cache.at(m_cache_key_colr);
      program.bind();
      program.bind("b_buff_unif",        m_unif_buffer);
      program.bind("b_buff_uplift_data", e_tesselation_data);
      program.bind("b_buff_uplift_pack", e_tesselation_pack);
      program.bind("b_buff_uplift_coef", e_tesselation_coef);
      program.bind("b_buff_objects",     e_scene.components.objects.gl.object_info);
      program.bind("b_buff_atlas",       e_barycentrics.buffer());
      if (std::holds_alternative<uint>(e_object.diffuse) && !e_scene.resources.images.empty()) {
        program.bind("b_txtr_3f",        e_scene.resources.images.gl.texture_atlas_3f.texture());
        program.bind("b_buff_textures",  e_scene.resources.images.gl.texture_info);
      }

      // Coordinates address the full atlas texture, while we enable a scissor 
      // test to restrict all operations in this scope to the relevant texture patch
      gl::state::ScopedSet scope(gl::DrawCapability::eScissorTest, true);
      gl::state::set(gl::DrawCapability::eScissorTest, true);
      gl::state::set_scissor(e_patch.size, e_patch.offs);
      gl::state::set_viewport(e_barycentrics.texture().size().head<2>());

      // Prepare framebuffer, clear relevant patch (not necessary actually)
      m_fbo.bind();
      m_fbo.clear(gl::FramebufferType::eColor, 0, 0);
      m_fbo.clear(gl::FramebufferType::eColor, 0, 1);

      // Find relevant draw command to map UVs;
      // if no UVs are present, we fall back on a rectangle's UVs to simply fill the patch
      gl::MultiDrawInfo::DrawCommand command;
      if (e_scene.resources.meshes[e_object.mesh_i].value().has_txuvs()) {
        command = e_scene.resources.meshes.gl.draw_commands[e_object.mesh_i];
      } else {
        command = e_scene.resources.meshes.gl.draw_commands[0]; // Rectangle
      }

      // Dispatch draw call
      gl::sync::memory_barrier(gl::BarrierFlags::eFramebuffer       | 
                               gl::BarrierFlags::eTextureFetch       |
                               gl::BarrierFlags::eClientMappedBuffer |
                               gl::BarrierFlags::eStorageBuffer      | 
                               gl::BarrierFlags::eUniformBuffer      );
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

    { // Second dispatch, bake spectral moment coefficients
      auto &program = e_cache.at(m_cache_key_bake);


    }
  }
} // namespace met