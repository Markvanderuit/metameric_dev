#include <metameric/core/data.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/viewport/task_draw_texture.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/utility.hpp>
#include <algorithm>
#include <execution>
#include <ranges>
#include <vector>
#include <unordered_set>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  void ViewportDrawTextureTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_texture = info.global("appl_data").getr<ApplicationData>().loaded_texture;

    // Generate 8-bit packed texture data with identical elements stripped
    std::unordered_set<uint> packed_set;
    std::ranges::transform(e_texture.data(), std::inserter(packed_set, packed_set.begin()), [](const auto &v_) {
      eig::Array3u v = (v_.max(0.f).min(1.f) * 255.f).round().cast<uint>().eval();
      return v[0] | (v[1] << 8) | (v[2] << 16);
    });
    std::vector<uint> packed_data(range_iter(packed_set));

    // Setup buffers and buffer mappings
    m_data_buffer    = {{ .data = cnt_span<const std::byte>(packed_data) }};
    m_uniform_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_uniform_map    = m_uniform_buffer.map_as<UniformBuffer>(buffer_access_flags).data();

    // Setup program for billboard point draw
    m_program = {{ .type       = gl::ShaderType::eVertex,   
                   .spirv_path = "resources/shaders/views/draw_texture.vert.spv",
                   .cross_path = "resources/shaders/views/draw_texture.vert.json" },
                 { .type       = gl::ShaderType::eFragment, 
                   .spirv_path = "resources/shaders/views/draw_texture.frag.spv",
                   .cross_path = "resources/shaders/views/draw_texture.frag.json" }};

    // Specify array and draw object
    m_array = {{ }};
    m_draw = {
      .type             = gl::PrimitiveType::eTriangles,
      .vertex_count     = 3 * static_cast<uint>(m_data_buffer.size() / sizeof(uint)),
      .draw_op          = gl::DrawOp::eFill,
      .bindable_array   = &m_array,
      .bindable_program = &m_program
    };
  }

  void ViewportDrawTextureTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources 
    const auto &e_view_state = info.resource("state", "view_state").getr<ViewportState>();

    // Declare scoped OpenGL state
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,     false),
                               gl::state::ScopedSet(gl::DrawCapability::eCullOp,    true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true) };
    
    // Set varying program uniforms
    if (e_view_state.camera_matrix || e_view_state.camera_aspect) {
      const auto &e_arcball = info("viewport.input", "arcball").getr<detail::Arcball>();
      m_uniform_map->camera_matrix = e_arcball.full().matrix();
      m_uniform_map->camera_aspect = { 1.f, e_arcball.aspect() };
      m_uniform_buffer.flush();
    }

    // Bind resources and submit draw
    m_program.bind("b_unif_buffer", m_uniform_buffer);
    m_program.bind("b_data_buffer", m_data_buffer);
    // m_program.bind("b_erro_buffer", info.resource("error_viewer", "colr_buffer").getr<gl::Buffer>());
    gl::dispatch_draw(m_draw);
  }
} // namespace met