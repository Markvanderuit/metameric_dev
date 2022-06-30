#include <metameric/core/io.hpp>
#include <metameric/gui/task/mapping_task.hpp>
#include <ranges>

namespace met {
  namespace detail {
    Spec eval_grid(uint grid_size, const std::span<Spec> &grid, Color p) {
      auto eval_grid_u = [&](const eig::Vector3i &u) {
        int i = u.z() * std::pow<uint>(grid_size, 2) + u.y() * grid_size + u.x();
        return grid[i];
      };
      constexpr auto lerp = [](const auto &a, const auto &b, float t) {
        return a + t * (b - a);
      };

      // Compute nearest positions and an alpha component for interpolation
      p = p.max(0.f).min(1.f) * static_cast<float>(grid_size - 1);
      eig::Array3i lo = p.floor().cast<int>(), up = p.ceil().cast<int>();
      eig::Array3f alpha = p - lo.cast<float>();

      // Sample the eight nearest positions in the grid
      auto lll = eval_grid_u({ lo[0], lo[1], lo[2] }), ull = eval_grid_u({ up[0], lo[1], lo[2] }),
           lul = eval_grid_u({ lo[0], up[1], lo[2] }), llu = eval_grid_u({ lo[0], lo[1], up[2] }),
           uul = eval_grid_u({ up[0], up[1], lo[2] }), luu = eval_grid_u({ lo[0], up[1], up[2] }),
           ulu = eval_grid_u({ up[0], lo[1], up[2] }), uuu = eval_grid_u({ up[0], up[1], up[2] });

      // Return trilinear interpolation
      return lerp(lerp(lerp(lll, ull, alpha[0]), lerp(lul, uul, alpha[0]), alpha[1]),
                  lerp(lerp(llu, ulu, alpha[0]), lerp(luu, uuu, alpha[0]), alpha[1]), alpha[2]);
    }
  } // namespace details

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
    gl::Buffer texture_buffer = {{ .size = sizeof(Spec) * glm::prod(color_texture_data.size), .flags = create_flags }};
    std::span<Spec> texture_map = convert_span<Spec>(texture_buffer.map(map_flags));
    info.insert_resource("spectral_texture_buffer", std::move(texture_buffer));
    info.insert_resource("spectral_texture_map", std::move(texture_map));

    // Create spectral gamut buffer and map here for now
    gl::Buffer gamut_buffer = {{ .size = sizeof(Spec) * 4, .flags = create_flags }};
    std::span<Spec> gamut_map = convert_span<Spec>(gamut_buffer.map(map_flags));
    info.insert_resource("spectral_gamut_buffer", std::move(gamut_buffer));
    info.insert_resource("spectral_gamut_map", std::move(gamut_map));

    uint dispatch_n = ceil_div(glm::prod(color_texture_data.size), 256);

    // Initialize objects for spectral texture gen.
    m_generate_program = {{ .type = gl::ShaderType::eCompute,
                            .path = "resources/shaders/mapping_task/generate_spectral_texture.comp" }};
    m_generate_program.uniform<uint>("u_n", dispatch_n);
    m_generate_dispatch = { .groups_x         = dispatch_n,
                            .bindable_program = &m_generate_program };

    // Initialize objects for color texture gen.
    m_mapping_program = {{ .type = gl::ShaderType::eCompute,
                           .path = "resources/shaders/mapping_task/apply_color_mapping.comp" }};
    m_mapping_program.uniform<uint>("u_n", dispatch_n);
    m_mapping_dispatch = { .groups_x         = dispatch_n,
                           .bindable_program = &m_mapping_program };
  }

  void MappingTask::eval(detail::TaskEvalInfo &info) {
    // Get externally shared resources
    auto &e_color_gamut_buffer      = info.get_resource<gl::Buffer>("global", "color_gamut_buffer");
    auto &e_color_gamut_map         = info.get_resource<std::span<Color>>("global", "color_gamut_map");
    auto &e_spectral_grid           = info.get_resource<std::span<Spec>>("global", "spectral_grid");
    auto &e_color_texture_buffer    = info.get_resource<gl::Buffer>("global", "color_texture_buffer_gpu");
    auto &i_spectral_texture_buffer = info.get_resource<gl::Buffer>("spectral_texture_buffer");
    auto &i_spectral_gamut_map      = info.get_resource<std::span<Spec>>("spectral_gamut_map");
    auto &i_spectral_gamut_buffer   = info.get_resource<gl::Buffer>("spectral_gamut_buffer");

    // Sample spectra at gamut corner positions
    constexpr uint grid_size = 64;
    std::ranges::transform(e_color_gamut_map, i_spectral_gamut_map.begin(),
      [&](const auto &p) { return detail::eval_grid(grid_size, e_spectral_grid, p); });

    // Generate spectral texture
    e_color_gamut_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    i_spectral_gamut_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    e_color_texture_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 2);
    i_spectral_texture_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 3);
    gl::dispatch_compute(m_generate_dispatch);

    // Generate color texture
    // TODO: need buffer first
  }
} // namespace met