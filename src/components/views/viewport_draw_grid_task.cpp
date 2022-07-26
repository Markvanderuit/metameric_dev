#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <metameric/core/knn.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/viewport_draw_grid_task.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <execution>

namespace met {
  
  ViewportDrawGridTask::ViewportDrawGridTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void ViewportDrawGridTask::init(detail::TaskInitInfo &info) {
    // Get externally shared resources
    auto &e_vox_grid = info.get_resource<ApplicationData>(global_key, "app_data").spec_vox_grid;

    // Obtain aligned D65 color of all voxels in the spectral grid
    std::vector<eig::AlArray3f> color_grid(e_vox_grid.data().size());
    std::transform(std::execution::par_unseq, 
      e_vox_grid.data().begin(), e_vox_grid.data().end(), color_grid.begin(), 
      [](const Spec &s) { return reflectance_to_color(s, { .cmfs = models::cmfs_srgb }); });

    // Construct buffer object and draw components
    m_vertex_buffer = {{ .data = as_span<const std::byte>(color_grid) }};
    m_vertex_array = {{
      .buffers = {{ .buffer = &m_vertex_buffer, .index = 0, .stride = sizeof(eig::AlArray3f) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }}
    }};
    m_program = {{ .type = gl::ShaderType::eVertex, 
                   .path = "resources/shaders/viewport_task/value_draw.vert" },
                 { .type = gl::ShaderType::eFragment,  
                   .path = "resources/shaders/viewport_task/vec3_passthrough.frag" }};
    m_draw = { .type             = gl::PrimitiveType::ePoints,
               .vertex_count     = (uint) color_grid.size(),
               .bindable_array   = &m_vertex_array,
               .bindable_program = &m_program };
  }

  void ViewportDrawGridTask::eval(detail::TaskEvalInfo &info) {
    // Insert temporary window to modify draw settings
    if (ImGui::Begin("Grid draw settings")) {
      ImGui::SliderFloat("Grid point size", &m_psize, 1.f, 32.f, "%.0f");
    }
    ImGui::End();

    // Get externally shared resources 
    auto &e_frame_buffer = info.get_resource<gl::Framebuffer>("viewport_draw_begin", "frame_buffer_msaa");
    auto &e_draw_texture = info.get_resource<gl::Texture2d3f>("viewport", "draw_texture");
    auto &e_arcball      = info.get_resource<detail::Arcball>("viewport", "arcball");
    auto &e_model_matrix = info.get_resource<glm::mat4>("viewport", "model_matrix");

    // Declare scoped OpenGL state
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA, true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true) };

    // Prepare framebuffer as draw target
    e_frame_buffer.bind();
    gl::state::set_viewport(e_draw_texture.size());
    
    // Update program uniforms
    m_program.uniform("u_model_matrix",  e_model_matrix);
    m_program.uniform("u_camera_matrix", e_arcball.full());    

    // Dispatch draw call
    gl::state::set_point_size(m_psize);
    gl::dispatch_draw(m_draw);
  }
} // namespace met