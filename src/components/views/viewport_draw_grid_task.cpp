#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/knn.hpp>
#include <metameric/core/spectrum.hpp>
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
    auto &e_spectral_vxl_grid  = info.get_resource<VoxelGrid<Spec>>("global", "spectral_voxel_grid");

    // Obtain aligned D65 color of all voxels in the spectral grid
    std::vector<eig::AlArray3f> color_grid(e_spectral_vxl_grid.data().size());
    std::transform(std::execution::par_unseq, 
      e_spectral_vxl_grid.data().begin(), e_spectral_vxl_grid.data().end(), color_grid.begin(), 
      [](const Spec &s) { return reflectance_to_color(s, { .cmfs = models::cmfs_srgb }); });

    // Construct buffer object and draw components
    m_grid_vertex_buffer = {{ .data = as_typed_span<const std::byte>(color_grid) }};
    m_grid_array = {{
      .buffers = {{ .buffer = &m_grid_vertex_buffer, .index = 0, .stride = sizeof(eig::AlArray3f) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }}
    }};
    m_grid_program = {{ .type = gl::ShaderType::eVertex, 
                        .path = "resources/shaders/viewport_task/value_draw.vert" },
                      { .type = gl::ShaderType::eFragment,  
                        .path = "resources/shaders/viewport_task/vec3_passthrough.frag" }};
    m_grid_draw = { .type             = gl::PrimitiveType::ePoints,
                    .vertex_count     = (uint) color_grid.size(),
                    .bindable_array   = &m_grid_array,
                    .bindable_program = &m_grid_program };
  }

  void ViewportDrawGridTask::eval(detail::TaskEvalInfo &info) {
    // Insert temporary window to modify draw settings
    if (ImGui::Begin("Grid draw settings")) {
      ImGui::SliderFloat("Grid point size", &m_grid_psize, 1.f, 32.f, "%.0f");
    }
    ImGui::End();

    // Get externally shared resources 
    auto &e_viewport_texture      = info.get_resource<gl::Texture2d3f>("viewport", "viewport_texture");
    auto &e_viewport_arcball      = info.get_resource<detail::Arcball>("viewport", "viewport_arcball");
    auto &e_viewport_model_matrix = info.get_resource<glm::mat4>("viewport", "viewport_model_matrix");
    auto &e_viewport_fbuffer      = info.get_resource<gl::Framebuffer>("viewport_draw_begin", "viewport_fbuffer_msaa");

    // Declare scoped OpenGL state
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA, true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true) };

    // Prepare multisampled framebuffer as draw target
    e_viewport_fbuffer.bind();
    gl::state::set_viewport(e_viewport_texture.size());
    
    // Update program uniforms
    m_grid_program.uniform("u_model_matrix",  e_viewport_model_matrix);
    m_grid_program.uniform("u_camera_matrix", e_viewport_arcball.full());    

    // Dispatch draw call
    gl::state::set_point_size(m_grid_psize);
    gl::dispatch_draw(m_grid_draw);
  }
} // namespace met