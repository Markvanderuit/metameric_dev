#include <metameric/core/data.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/viewport/task_draw_texture.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  ViewportDrawTextureTask::ViewportDrawTextureTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportDrawTextureTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_texture_data   = info.get_resource<ApplicationData>(global_key, "app_data").loaded_texture_f32;

    // Setup program for instanced billboard point draw
    m_program = {{ .type = gl::ShaderType::eVertex,   .path = "resources/shaders/viewport/draw_texture_inst.vert.spv_opt", .is_spirv_binary = true },
                 { .type = gl::ShaderType::eFragment, .path = "resources/shaders/viewport/draw_texture.frag.spv_opt", .is_spirv_binary = true }};

    // Specify array and draw object
    m_array = {{ }};
    m_draw = {
      .type             = gl::PrimitiveType::eTriangles,
      .vertex_count     = 3 * e_texture_data.size().prod(),
      .draw_op          = gl::DrawOp::eFill,
      .bindable_array   = &m_array,
      .bindable_program = &m_program
    };
  }

  void ViewportDrawTextureTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources 
    auto &e_arcball     = info.get_resource<detail::Arcball>("viewport_input", "arcball");
    auto &e_pack_buffer = info.get_resource<gl::Buffer>("gen_delaunay_weights", "pack_buffer");
    auto &e_err_buffer  = info.get_resource<gl::Buffer>("error_viewer", "colr_buffer");

    // Declare scoped OpenGL state
    gl::state::set_op(gl::CullOp::eBack);
    gl::state::set_op(gl::DepthOp::eLess);
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,     false),
                               gl::state::ScopedSet(gl::DrawCapability::eCullOp,    true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true) };
    
    // Set varying program uniforms
    m_program.uniform("u_camera_matrix",    e_arcball.full().matrix());
    m_program.uniform("u_billboard_aspect", eig::Vector2f { 1.f, e_arcball.m_aspect });

    // Bind resources to buffer targets
    e_pack_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    e_err_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);

    // Submit draw information
    gl::dispatch_draw(m_draw);
  }
} // namespace met