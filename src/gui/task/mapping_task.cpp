#include <metameric/core/io.hpp>
#include <metameric/core/spectral_grid.hpp>
#include <metameric/gui/task/mapping_task.hpp>
#include <small_gl/utility.hpp>
#include <fmt/ranges.h>
#include <ranges>

namespace met {
  MappingTask::MappingTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void MappingTask::init(detail::TaskInitInfo &info) {
    constexpr auto create_flags
      = gl::BufferCreateFlags::eMapRead | gl::BufferCreateFlags::eMapWrite 
      | gl::BufferCreateFlags::eMapPersistent | gl::BufferCreateFlags::eMapCoherent;
    constexpr auto map_flags 
      = gl::BufferAccessFlags::eMapRead | gl::BufferAccessFlags::eMapWrite
      | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapCoherent;
      
    // Get externally shared resources
    auto &color_texture_data = info.get_resource<io::TextureData<float>>("global", "color_texture_buffer_cpu");

    // Create spectral texture buffer and map here for now
    float generate_clear_value = 1.f;
    m_generate_buffer = {{ .size = sizeof(Spec) * glm::prod(color_texture_data.size), .flags = create_flags }};
    m_generate_buffer.clear(std::span { (const std::byte *) &generate_clear_value, sizeof(float) });

    // Create debug buffer
    float debug_clear_value = .5f;
    m_debug_buffer = {{ .size = sizeof(eig::Matrix4f), .flags = create_flags }};
    m_debug_buffer.clear(std::span { (const std::byte *) &debug_clear_value, sizeof(float) });

    // Create spectral gamut buffer and map here for now
    gl::Buffer gamut_buffer = {{ .size = sizeof(Spec) * 4, .flags = create_flags }};
    std::span<Spec> gamut_map = convert_span<Spec>(gamut_buffer.map(map_flags));
    info.insert_resource("spectral_gamut_buffer", std::move(gamut_buffer));
    info.insert_resource("spectral_gamut_map", std::move(gamut_map));

    const uint generate_n    = glm::prod(color_texture_data.size);
    const uint generate_ndiv = ceil_div(generate_n, 256u);

    // Initialize objects for spectral texture gen.
    m_generate_program = {{ .type = gl::ShaderType::eCompute,
                            .path = "resources/shaders/mapping_task/generate_spectral_texture.comp" }};
    m_generate_dispatch = { .groups_x = generate_ndiv, .bindable_program = &m_generate_program };

    // Temporary object to describe circumstances for spectral to rgb conversion
    struct MappingType {
      CMFS cmfs          = models::cmfs_srgb.transpose().reshaped(3, wavelength_samples); // TODO this is a problem
      Spec illuminant    = models::emitter_cie_d65;
      uint n_scatterings = 0;
    } mapping;
    // const SpectralMapping mapping;

    const uint mapping_n    = glm::prod(color_texture_data.size);
    const uint mapping_ndiv = ceil_div(mapping_n, 256u); 

    // Initialize objects for color texture gen.
    m_mapping_buffer  = {{ .data = std::span { (const std::byte *) &mapping, sizeof(mapping) }}};
    m_mapping_texture = {{ .size = (size_t) glm::prod(color_texture_data.size) * sizeof(glm::vec4) }};
    m_mapping_program = {{ .type = gl::ShaderType::eCompute,
                           .path = "resources/shaders/mapping_task/apply_color_mapping.comp" }};
    m_mapping_dispatch = { .groups_x = mapping_ndiv, .bindable_program = &m_mapping_program };

    glm::uvec2 texture_n    = color_texture_data.size;
    glm::uvec2 texture_ndiv = ceil_div(texture_n, glm::uvec2(16));

    // Initialize objects for buffer-to-texture conversion
    m_texture_program = {{ .type = gl::ShaderType::eCompute,
                           .path = "resources/shaders/mapping_task/buffer_to_texture.comp" }};
    m_texture_dispatch = { .groups_x = texture_ndiv.x,
                           .groups_y = texture_ndiv.y,
                           .bindable_program = &m_texture_program };

    // Set these uniforms once
    m_generate_program.uniform<uint>("u_n", generate_n);
    m_mapping_program.uniform<uint>("u_n", mapping_n);
    m_texture_program.uniform<glm::uvec2>("u_size", texture_n);

    //  Create texture target for this task
    gl::Texture2d4f color_texture = {{ .size = color_texture_data.size }};
    info.insert_resource("color_texture", std::move(color_texture));
  }

  void MappingTask::eval(detail::TaskEvalInfo &info) {
    // Get externally shared resources
    auto &e_color_gamut_buffer   = info.get_resource<gl::Buffer>("global", "color_gamut_buffer");
    auto &e_color_texture_buffer = info.get_resource<gl::Buffer>("global", "color_texture_buffer_gpu");
    auto &e_spectral_knn_grid  = info.get_resource<KNNGrid<Spec>>("global", "spectral_knn_grid");

    // Get internally shared resources
    auto &i_spectral_gamut_map    = info.get_resource<std::span<Spec>>("spectral_gamut_map");
    auto &i_spectral_gamut_buffer = info.get_resource<gl::Buffer>("spectral_gamut_buffer");
    auto &i_color_texture         = info.get_resource<gl::Texture2d4f>("color_texture");

    // Generate temporary mapping to color gamut buffer 
    constexpr auto map_flags = gl::BufferAccessFlags::eMapRead | gl::BufferAccessFlags::eMapWrite;
    auto color_gamut_map = convert_span<Color>(e_color_gamut_buffer.map(map_flags));

    // Sample spectra at gamut corner positions
    std::ranges::transform(color_gamut_map, i_spectral_gamut_map.begin(),
    [&](const auto &p) { return e_spectral_knn_grid.query_1_nearest(p).value; });

    // Close buffer mapping
    e_color_gamut_buffer.unmap();

    // Generate spectral texture buffer
    e_color_gamut_buffer.bind_to(gl::BufferTargetType::eShaderStorage,    0);
    i_spectral_gamut_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    e_color_texture_buffer.bind_to(gl::BufferTargetType::eShaderStorage,  2);
    m_generate_buffer.bind_to(gl::BufferTargetType::eShaderStorage,       3);
    m_debug_buffer.bind_to(gl::BufferTargetType::eShaderStorage,          4);
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_generate_dispatch);

    // Generate color texture buffer
    m_generate_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    m_mapping_buffer.bind_to(gl::BufferTargetType::eShaderStorage,  1);
    m_mapping_texture.bind_to(gl::BufferTargetType::eShaderStorage, 2);
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_mapping_dispatch);

    // Render to texture
    m_mapping_texture.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    i_color_texture.bind_to(gl::TextureTargetType::eImageWriteOnly, 0);
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_texture_dispatch);

    // Inspect debug buffer
    eig::Matrix4f debug_value = 0.f;
    m_debug_buffer.get(as_typed_span<std::byte>(debug_value));
    eig::Matrix3f debug_out = debug_value.topLeftCorner(2, 2);
    fmt::print("gpu - {}\n", as_typed_span<float>(debug_out));
  }
} // namespace met