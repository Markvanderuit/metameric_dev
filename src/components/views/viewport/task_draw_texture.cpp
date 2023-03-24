#include <metameric/core/data.hpp>
#include <metameric/core/spectrum.hpp>
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
  void ViewportDrawTextureTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_texture = info.global("app_data").read_only<ApplicationData>().loaded_texture_f32;

    // Generate 8-bit packed texture data with identical elements stripped
    std::unordered_set<uint> packed_set;
    std::ranges::transform(e_texture.data(), std::inserter(packed_set, packed_set.begin()), [](const auto &v_) {
      eig::Array3u v = (v_.max(0.f).min(1.f) * 255.f).round().cast<uint>().eval();
      return v[0] | (v[1] << 8) | (v[2] << 16);
    });
    std::vector<uint> packed_data(range_iter(packed_set));
    m_data = {{ .data = cnt_span<const std::byte>(packed_data) }};

    // Setup program for instanced billboard point draw
    m_program = {{ .type = gl::ShaderType::eVertex,   .path = "resources/shaders/viewport/draw_texture_inst.vert.spv_opt", .is_spirv_binary = true },
                 { .type = gl::ShaderType::eFragment, .path = "resources/shaders/viewport/draw_texture.frag.spv_opt", .is_spirv_binary = true }};

    // Specify array and draw object
    m_array = {{ }};
    m_draw = {
      .type             = gl::PrimitiveType::eTriangles,
      .vertex_count     = 3 * static_cast<uint>(m_data.size() / sizeof(uint)),
      .draw_op          = gl::DrawOp::eFill,
      .bindable_array   = &m_array,
      .bindable_program = &m_program
    };
  }

  void ViewportDrawTextureTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources 
    const auto &e_arcball    = info.resource("viewport.input", "arcball").read_only<detail::Arcball>();
    const auto &e_err_buffer = info.resource("error_viewer", "colr_buffer").read_only<gl::Buffer>();

    // Declare scoped OpenGL state
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,     false),
                               gl::state::ScopedSet(gl::DrawCapability::eCullOp,    true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true) };
    
    // Set varying program uniforms
    m_program.uniform("u_camera_matrix",    e_arcball.full().matrix());
    m_program.uniform("u_billboard_aspect", eig::Vector2f { 1.f, e_arcball.m_aspect });

    // Bind resources to buffer targets
    m_data.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    e_err_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);

    // Submit draw information
    gl::dispatch_draw(m_draw);
  }
} // namespace met