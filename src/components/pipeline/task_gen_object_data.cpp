#include <metameric/scene/scene.hpp>
#include <metameric/components/pipeline/task_gen_object_data.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/detail/program_cache.hpp>
#include <format>

namespace met {
  GenObjectDataTask:: GenObjectDataTask(uint object_i)
  : m_object_i(object_i),
    m_coef_layer_i(0),
    m_brdf_layer_i(0) { }

  bool GenObjectDataTask::is_active(SchedulerHandle &info) {
    met_trace();

    // Get external resources
    const auto &e_scene     = info.global("scene").getr<Scene>();
    const auto &e_object    = e_scene.components.objects[m_object_i];
    const auto &e_uplifting = e_scene.components.upliftings[e_object->uplifting_i];
    const auto &e_settings  = e_scene.components.settings;
    const auto &e_coef      = e_scene.components.upliftings.gl.texture_coef;
    const auto &e_brdf      = e_scene.components.upliftings.gl.texture_brdf;

    // Careful management of internal state; "careful" beinng synonymous with "ewwww"
    return is_first_eval()                 || // First run demands render anyways
           e_coef.is_invalitated()         || // Texture atlas re-allocate demands re-render
           e_brdf.is_invalitated()         || // Texture atlas re-allocate demands re-render
           e_object.state.diffuse          || // Object brdf parameter was changed
           e_object.state.roughness        || // Object brdf parameter was changed
           e_object.state.metallic         || // Object brdf parameter was changed
           e_object.state.mesh_i           || // Object mesh was changed to a different onen
           e_object.state.uplifting_i      || // Object uplifting was changed to a different onen
           e_uplifting.state               || // Object's selected uplifting was changed
           e_scene.resources.meshes        || // Generally; used loaded a mesh
           e_scene.resources.images        || // Generally; used loaded a image
           e_settings.state.texture_size   ;  // Used modified texture size setting
  }

  void GenObjectDataTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Initialize program objects in cache, if they're not loaded already. Get handles to objects
    auto &e_cache = info.global("cache").getw<gl::detail::ProgramCache>();
    std::tie(m_coef_cache_key, std::ignore) = e_cache.set(
     {{ .type       = gl::ShaderType::eVertex,
        .spirv_path = "resources/shaders/pipeline/gen_texture.vert.spv",
        .cross_path = "resources/shaders/pipeline/gen_texture.vert.json" },
      { .type       = gl::ShaderType::eFragment,
        .spirv_path = "resources/shaders/pipeline/gen_texture_coef.frag.spv",
        .cross_path = "resources/shaders/pipeline/gen_texture_coef.frag.json" }});
    std::tie(m_brdf_cache_key, std::ignore) = e_cache.set(
     {{ .type       = gl::ShaderType::eVertex,
        .spirv_path = "resources/shaders/pipeline/gen_texture.vert.spv",
        .cross_path = "resources/shaders/pipeline/gen_texture.vert.json" },
      { .type       = gl::ShaderType::eFragment,
        .spirv_path = "resources/shaders/pipeline/gen_texture_brdf.frag.spv",
        .cross_path = "resources/shaders/pipeline/gen_texture_brdf.frag.json" }});
                                
    // Initialize uniform buffers and writeable, flushable mappings
    std::tie(m_coef_unif, m_coef_unif_map) = gl::Buffer::make_flusheable_object<UnifLayout>();
    m_coef_unif_map->object_i = m_object_i;
    m_coef_unif_map->px_scale = 1.f;
    m_coef_unif.flush();
    std::tie(m_brdf_unif, m_brdf_unif_map) = gl::Buffer::make_flusheable_object<UnifLayout>();
    m_brdf_unif_map->object_i = m_object_i;
    m_brdf_unif_map->px_scale = 1.f;
    m_brdf_unif.flush();

    // Linear texture sampler
    m_sampler = {{ .min_filter = gl::SamplerMinFilter::eLinear, .mag_filter = gl::SamplerMagFilter::eLinear }};
  }

  void GenObjectDataTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
    // Get external resources
    auto       &e_cache     = info.global("cache").getw<gl::detail::ProgramCache>();
    const auto &e_scene     = info.global("scene").getr<Scene>();
    const auto &e_object    = e_scene.components.objects[m_object_i].value;
    const auto &e_mesh      = e_scene.resources.meshes[e_object.mesh_i].value();
    const auto &e_uplifting = e_scene.components.upliftings[e_object.uplifting_i];

    // Handle coefficient texture baking
    { 
      // Find relevant patch in the texture atlas
      const auto &e_coef  = e_scene.components.upliftings.gl.texture_coef;
      const auto &e_patch = e_coef.patch(m_object_i);

      // Rebuild framebuffer if necessary
      if (is_first_eval() || e_coef.is_invalitated() || m_coef_layer_i != e_patch.layer_i) {
        m_coef_layer_i = e_patch.layer_i;
        m_coef_fbo = {{ .type       = gl::FramebufferType::eColor,
                        .attachment = &e_coef.texture(),
                        .layer      = m_coef_layer_i }};
      } 

      // Get external resources from object's corresponding, selected uplifting
      // An object has only one uplifting structure associated to it,
      // but several objects can reuse the same uplifting
      auto uplifting_task_name       = std::format("gen_upliftings.gen_uplifting_{}", e_object.uplifting_i);
      const auto &e_tesselation_data = info(uplifting_task_name, "tesselation_data").getr<gl::Buffer>();
      const auto &e_tesselation_pack = info(uplifting_task_name, "tesselation_pack").getr<gl::Buffer>();
      const auto &e_tesselation_coef = info(uplifting_task_name, "tesselation_coef").getr<gl::Buffer>();
      
      // Push data to uniform buffer
      m_coef_unif_map->px_scale = static_cast<float>(e_patch.size.prod()) 
                                / static_cast<float>(e_coef.texture().size().head<2>().prod());
      m_coef_unif.flush();
      
      // Get ref. to relevant program, then bind resources to corresponding targets
      auto &program = e_cache.at(m_coef_cache_key);
      program.bind();
      program.bind("b_buff_unif",        m_coef_unif);
      program.bind("b_buff_uplift_data", e_tesselation_data);
      program.bind("b_buff_uplift_pack", e_tesselation_pack);
      program.bind("b_buff_uplift_coef", e_tesselation_coef);
      program.bind("b_buff_object_info",     e_scene.components.objects.gl.object_info);
      program.bind("b_buff_atlas",       e_coef.buffer());
      if (!e_scene.resources.images.empty()) {
        program.bind("b_txtr_3f",        e_scene.resources.images.gl.texture_atlas_3f.texture());
        program.bind("b_txtr_3f",        m_sampler);  
        program.bind("b_txtr_1f",        e_scene.resources.images.gl.texture_atlas_1f.texture());
        program.bind("b_txtr_1f",        m_sampler);  
        program.bind("b_buff_textures",  e_scene.resources.images.gl.texture_info);
      }

      // Coordinates address the full atlas texture, while we enable a scissor 
      // test to restrict all operations in this scope to the relevant texture patch
      gl::state::ScopedSet scope(gl::DrawCapability::eScissorTest, true);
      gl::state::set(gl::DrawCapability::eScissorTest, true);
      gl::state::set(gl::DrawCapability::eDither,     false);
      gl::state::set_scissor(e_patch.size, e_patch.offs);
      gl::state::set_viewport(e_coef.texture().size().head<2>());
      gl::state::set_line_width(4.f);

      // Prepare framebuffer, clear relevant patch (not actually necessary)
      m_coef_fbo.bind();
      m_coef_fbo.clear(gl::FramebufferType::eColor, 0u);

      // Find relevant draw command to map UVs;
      // if no UVs are present, we fall back on a rectangle's UVs to simply fill the patch
      gl::MultiDrawInfo::DrawCommand command;
      if (e_mesh.has_txuvs() && e_object.diffuse.index() == 1) {
        command = e_scene.resources.meshes.gl.mesh_draw[e_object.mesh_i];
      } else {
        command = e_scene.resources.meshes.gl.mesh_draw[0]; // Rectangle
      }

      // Insert barrier for previous elements
      gl::sync::memory_barrier(gl::BarrierFlags::eFramebuffer        | 
                               gl::BarrierFlags::eTextureFetch       |
                               gl::BarrierFlags::eClientMappedBuffer |
                               gl::BarrierFlags::eStorageBuffer      | 
                               gl::BarrierFlags::eUniformBuffer      );
                               
      // Triangle fill for most data
      gl::dispatch_draw({
        .type           = gl::PrimitiveType::eTriangles,
        .vertex_count   = command.vertex_count,
        .vertex_first   = command.vertex_first,
        .capabilities   = {{ gl::DrawCapability::eDepthTest, false },
                           { gl::DrawCapability::eCullOp,    false },
                           { gl::DrawCapability::eBlendOp,   false }},
        .draw_op        = gl::DrawOp::eFill,
        .bindable_array = &e_scene.resources.meshes.gl.mesh_array
      });
      
      // Line fill to overlap boundaries
      gl::dispatch_draw({
        .type           = gl::PrimitiveType::eTriangles,
        .vertex_count   = command.vertex_count,
        .vertex_first   = command.vertex_first,
        .capabilities   = {{ gl::DrawCapability::eDepthTest, false },
                           { gl::DrawCapability::eCullOp,    false },
                           { gl::DrawCapability::eBlendOp,   false }},
        .draw_op        = gl::DrawOp::eLine,
        .bindable_array = &e_scene.resources.meshes.gl.mesh_array
      });
    }

    // Handle brdf parameter baking
    {
      // Find relevant patch in the texture atlas
      const auto &e_brdf  = e_scene.components.upliftings.gl.texture_brdf;
      const auto &e_patch = e_brdf.patch(m_object_i);
      
      // Rebuild framebuffer if necessary
      if (is_first_eval() || e_brdf.is_invalitated() || m_brdf_layer_i != e_patch.layer_i) {
        m_brdf_layer_i = e_patch.layer_i;
        m_brdf_fbo = {{ .type       = gl::FramebufferType::eColor,
                        .attachment = &e_brdf.texture(),
                        .layer      = m_brdf_layer_i }};
      } 
      
      // Push data to uniform buffer
      m_brdf_unif_map->px_scale = static_cast<float>(e_patch.size.prod()) 
                                / static_cast<float>(e_brdf.texture().size().head<2>().prod());
      m_brdf_unif.flush();

      // Get ref. to relevant program, then bind resources to corresponding targets
      auto &program = e_cache.at(m_brdf_cache_key);
      program.bind();
      program.bind("b_buff_unif",        m_brdf_unif);
      program.bind("b_buff_object_info",     e_scene.components.objects.gl.object_info);
      program.bind("b_buff_atlas",       e_brdf.buffer());
      if (!e_scene.resources.images.empty()) {
        program.bind("b_txtr_3f",        e_scene.resources.images.gl.texture_atlas_3f.texture());
        program.bind("b_txtr_3f",        m_sampler);  
        program.bind("b_txtr_1f",        e_scene.resources.images.gl.texture_atlas_1f.texture());
        program.bind("b_txtr_1f",        m_sampler);  
        program.bind("b_buff_textures",  e_scene.resources.images.gl.texture_info);
      }
      
      // Coordinates address the full atlas texture, while we enable a scissor 
      // test to restrict all operations in this scope to the relevant texture patch
      gl::state::ScopedSet scope(gl::DrawCapability::eScissorTest, true);
      gl::state::set(gl::DrawCapability::eScissorTest, true);
      gl::state::set(gl::DrawCapability::eDither,     false);
      gl::state::set_scissor(e_patch.size, e_patch.offs);
      gl::state::set_viewport(e_brdf.texture().size().head<2>());
      gl::state::set_line_width(4.f);

      // Prepare framebuffer, clear relevant patch (not actually necessary)
      m_brdf_fbo.bind();
      m_brdf_fbo.clear(gl::FramebufferType::eColor, 0u);
      
      // Find relevant draw command to map UVs;
      // if no UVs are present, we fall back on a rectangle's UVs to simply fill the patch
      gl::MultiDrawInfo::DrawCommand command;
      if (e_mesh.has_txuvs() && e_object.roughness.index() == 1 || e_object.metallic.index() == 1) {
        command = e_scene.resources.meshes.gl.mesh_draw[e_object.mesh_i];
      } else {
        command = e_scene.resources.meshes.gl.mesh_draw[0]; // Rectangle
      }

      // Insert barrier for previous elements
      gl::sync::memory_barrier(gl::BarrierFlags::eFramebuffer        | 
                               gl::BarrierFlags::eTextureFetch       |
                               gl::BarrierFlags::eClientMappedBuffer |
                               gl::BarrierFlags::eStorageBuffer      | 
                               gl::BarrierFlags::eUniformBuffer      );
       
      // Triangle fill for most data
      gl::dispatch_draw({
        .type           = gl::PrimitiveType::eTriangles,
        .vertex_count   = command.vertex_count,
        .vertex_first   = command.vertex_first,
        .capabilities   = {{ gl::DrawCapability::eDepthTest, false },
                           { gl::DrawCapability::eCullOp,    false },
                           { gl::DrawCapability::eBlendOp,   false }},
        .draw_op        = gl::DrawOp::eFill,
        .bindable_array = &e_scene.resources.meshes.gl.mesh_array
      });
      
      // Line fill to overlap boundaries
      gl::dispatch_draw({
        .type           = gl::PrimitiveType::eTriangles,
        .vertex_count   = command.vertex_count,
        .vertex_first   = command.vertex_first,
        .capabilities   = {{ gl::DrawCapability::eDepthTest, false },
                           { gl::DrawCapability::eCullOp,    false },
                           { gl::DrawCapability::eBlendOp,   false }},
        .draw_op        = gl::DrawOp::eLine,
        .bindable_array = &e_scene.resources.meshes.gl.mesh_array
      });
    }
  }
} // namespace met