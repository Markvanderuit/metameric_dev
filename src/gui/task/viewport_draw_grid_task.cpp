#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/gui/application.hpp>
#include <metameric/gui/spectral_grid.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <metameric/gui/detail/arcball.hpp>
#include <metameric/gui//task/viewport_draw_grid_task.hpp>
#include <execution>

namespace met {
  
  ViewportDrawGridTask::ViewportDrawGridTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void ViewportDrawGridTask::init(detail::TaskInitInfo &info) {
    // Get externally shared resources
    auto &e_spectral_grid      = info.get_resource<std::vector<Spec>>("global", "spectral_grid");
    auto &e_spectral_vxl_grid  = info.get_resource<VoxelGrid<Spec>>("global", "spectral_voxel_grid");
    auto &e_spectral_grid_size = info.get_resource<uint>("global", "spectral_grid_size");

    // Obtain padded D65 color of all voxels in the spectral grid
    std::vector<PaddedColor> color_grid(e_spectral_vxl_grid.data().size());
    std::transform(std::execution::par_unseq, 
      e_spectral_vxl_grid.data().begin(), e_spectral_vxl_grid.data().end(), color_grid.begin(), 
      [](const Spec &s) { return padd(xyz_to_srgb(reflectance_to_xyz(s))); });

    // std::vector<PaddedColor> color_grid(e_spectral_grid.size());
    // std::transform(std::execution::par_unseq, e_spectral_grid.begin(), e_spectral_grid.end(),
    //   color_grid.begin(), [](const Spec &s) { return padd(xyz_to_srgb(reflectance_to_xyz(s))); });
    
    // Construct buffer object and draw components
    m_grid_vertex_buffer = {{ .data = as_typed_span<const std::byte>(color_grid) }};
    m_grid_array = {{
      .buffers = {{ .buffer = &m_grid_vertex_buffer, .index = 0, .stride = sizeof(PaddedColor) }},
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