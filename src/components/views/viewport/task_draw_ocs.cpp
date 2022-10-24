#include <metameric/core/knn.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/viewport/task_draw_ocs.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <execution>

namespace met {
  ViewportDrawOCSTask::ViewportDrawOCSTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportDrawOCSTask::init(detail::TaskInitInfo &info) {
    met_trace_full();
    
    // Get shared resources
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_chull    = e_app_data.loaded_chull;

    // Construct draw components
    m_hull_vertices = {{ .data = cnt_span<const std::byte>(e_chull.verts()) }};
    m_hull_elements = {{ .data = cnt_span<const std::byte>(e_chull.elems()) }};
    m_hull_array = {{
      .buffers = {{ .buffer = &m_hull_vertices, .index = 0, .stride = sizeof(AlColr) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
      .elements = &m_hull_elements
    }};
    m_hull_dispatch = { .type = gl::PrimitiveType::eLines,
                        .vertex_count = (uint) (m_hull_elements.size() / sizeof(uint)),
                        .bindable_array = &m_hull_array,
                        .bindable_program = &m_program };
    m_program = {{ .type = gl::ShaderType::eVertex, 
                   .path = "resources/shaders/viewport/draw_color_array.vert" },
                 { .type = gl::ShaderType::eFragment,  
                   .path = "resources/shaders/viewport/draw_color_uniform_alpha.frag" }};

    // Set non-changing uniform values
    m_program.uniform("u_model_matrix",  eig::Matrix4f::Identity().eval());
    m_program.uniform("u_alpha", 0.33f);
  }

  void ViewportDrawOCSTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources 
    auto &e_arcball = info.get_resource<detail::Arcball>("viewport", "arcball");

    // Declare scoped OpenGL state
    gl::state::set_op(gl::CullOp::eBack);
    gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,      true),
                               gl::state::ScopedSet(gl::DrawCapability::eBlendOp,   true),
                               gl::state::ScopedSet(gl::DrawCapability::eCullOp,    true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest, false) };

    // Dispatch draw call
    m_program.uniform("u_camera_matrix", e_arcball.full().matrix());    
    gl::state::set_line_width(2.f);
    gl::dispatch_draw(m_hull_dispatch);
  }
} // namespace met