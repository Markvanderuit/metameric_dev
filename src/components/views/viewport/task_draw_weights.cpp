#include <metameric/components/views/viewport/task_draw_weights.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/data.hpp>
#include <metameric/core/texture.hpp>
#include <small_gl/utility.hpp>
#include <ranges>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

  DrawWeightsTask::DrawWeightsTask(const std::string &name, const std::string &parent)
  : detail::AbstractTask(name),
    m_parent(parent) { }
  
  void DrawWeightsTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_texture = info.get_resource<ApplicationData>(global_key, "app_data").loaded_texture;

    // Nr. of workgroups for sum computation and texture conversion
    const uint dispatch_n    = e_texture.size().prod();
    const uint dispatch_ndiv = ceil_div(dispatch_n, 256u / barycentric_weights);
    const eig::Array2u dispatch_texture_n    = e_texture.size();
    const eig::Array2u dispatch_texture_ndiv = ceil_div(dispatch_texture_n, 16u);

    // Initialize objects for shader call
    m_program = {{ .type = gl::ShaderType::eCompute,
                   .path = "resources/shaders/viewport/draw_weights.comp.spv_opt",
                   .is_spirv_binary = true }};
    m_texture_program = {{ .type = gl::ShaderType::eCompute,
                           .path = "resources/shaders/viewport/draw_weights_to_texture.comp.spv_opt",
                           .is_spirv_binary = true }};
    m_srgb_program = {{ .type = gl::ShaderType::eCompute, .path = "resources/shaders/misc/texture_resample.comp" }};
    
    // Create dispatch objects to summarize compute operations
    m_dispatch = { .groups_x = dispatch_ndiv, 
                   .bindable_program = &m_program }; 
    m_texture_dispatch = { .groups_x = dispatch_texture_ndiv.x(),
                           .groups_y = dispatch_texture_ndiv.y(), 
                           .bindable_program = &m_texture_program }; 
    m_srgb_dispatch = { .bindable_program = &m_srgb_program };

    // Create sampler object used in gamma correction step
    m_srgb_sampler = {{ .min_filter = gl::SamplerMinFilter::eLinear, .mag_filter = gl::SamplerMagFilter::eLinear }};

    // Set these uniforms once
    m_texture_program.uniform("u_size", dispatch_texture_n);
    m_srgb_program.uniform("u_sampler", 0);
    m_srgb_program.uniform("u_lrgb_to_srgb", true);

    // Initialize uniform buffer and writeable, flushable mapping
    m_unif_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_unif_map = &m_unif_buffer.map_as<UniformBuffer>(buffer_access_flags)[0];

    // Initialize buffer and texture for storing intermediate results
    m_buffer  = {{ .size = sizeof(float) * dispatch_n }};
    m_texture = {{ .size = dispatch_texture_n }};
    
    m_srgb_target_cache = 0;
    m_mapping_i_cache   = -1;
    m_selection_cache   = { };
  }

  void DrawWeightsTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();
    
    // Continue only on relevant state change
    auto &e_app_data    = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_prj_state   = e_app_data.project_state;
    auto &e_selection   = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");
    auto &e_mapping_i   = info.get_resource<uint>(m_parent, "weight_mapping");
    auto &e_srgb_target = info.get_resource<gl::Texture2d4f>(m_parent, "srgb_weights_target");
    guard(e_mapping_i != m_mapping_i_cache
      || e_srgb_target.object() != m_srgb_target_cache
      ||!std::ranges::equal(e_selection, m_selection_cache)
      || e_prj_state.any_verts);

    // Update local cache variables
    m_srgb_target_cache = e_srgb_target.object();
    m_selection_cache   = e_selection;
    m_mapping_i_cache   = e_mapping_i;
    
    // Continue only if a selection is currently active
    guard(m_mapping_i_cache >= 0 && !m_selection_cache.empty());

    // Get shared resources
    auto &e_bary_buffer = info.get_resource<gl::Buffer>("gen_barycentric_weights", "bary_buffer");
    auto &e_colr_buffer = info.get_resource<gl::Buffer>(fmt::format("gen_color_mapping_{}", e_mapping_i), "colr_buffer");

    // Update uniform data for upcoming sum computation
    m_unif_map->n       = e_app_data.loaded_texture.size().prod();
    m_unif_map->n_verts = e_app_data.project_data.gamut_verts.size();
    std::ranges::fill(m_unif_map->selection, eig::Array4u(0));
    std::ranges::for_each(e_selection, [&](uint i) { m_unif_map->selection[i] = 1; });
    m_unif_buffer.flush();

    // Update uniform/dispatch data for gamma correction/resampling computation
    eig::Array2u dispatch_n    = e_srgb_target.size();
    eig::Array2u dispatch_ndiv = ceil_div(e_srgb_target.size(), 16u);
    m_srgb_dispatch.groups_x = dispatch_ndiv.x();
    m_srgb_dispatch.groups_y = dispatch_ndiv.y();
    m_srgb_program.uniform("u_size", dispatch_n);

    // Bind resources to buffer targets for upcoming sum computation
    e_bary_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    m_buffer.bind_to(gl::BufferTargetType::eShaderStorage,      1);
    m_unif_buffer.bind_to(gl::BufferTargetType::eUniform,       0);

    // Dispatch shader to perform sum comutation
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer |
                             gl::BarrierFlags::eUniformBuffer       |
                             gl::BarrierFlags::eBufferUpdate        |
                             gl::BarrierFlags::eClientMappedBuffer);
    gl::dispatch_compute(m_dispatch);

    // Bind resources to buffer targets for upcoming buffer-to-texture conversion
    e_colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    m_buffer.bind_to(gl::BufferTargetType::eShaderStorage,      1);
    m_texture.bind_to(gl::TextureTargetType::eImageWriteOnly,   0);

    // Dispatch shader to move data into texture format
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer|
                             gl::BarrierFlags::eShaderImageAccess);
    gl::dispatch_compute(m_texture_dispatch);

    // Bind relevant resources to texture/image/sampler units for the coming compute operation
    m_texture.bind_to(gl::TextureTargetType::eTextureUnit,        0);
    e_srgb_target.bind_to(gl::TextureTargetType::eImageWriteOnly, 0);
    m_srgb_sampler.bind_to(0);

    // Dispatch shader to perform resampling and gamma correction
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer |
                             gl::BarrierFlags::eTextureFetch        |
                             gl::BarrierFlags::eShaderImageAccess);
    gl::dispatch_compute(m_srgb_dispatch);
  }
} // namespace met