#pragma once

#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <metameric/core/detail/glm.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/gui/detail/arcball.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>

namespace met {
  namespace detail {
    template <class T, class C>
    std::span<T> as_typed_span(C &c) {
      auto data = c.data();
      guard(data, {});
      return { reinterpret_cast<T*>(data), (c.size() * sizeof(C::value_type)) / sizeof(T) };
    }

    template <class T, class U>
    std::span<T> convert_span(std::span<U> s) {
      auto data = s.data();
      guard(data, {});
      return { reinterpret_cast<T*>(data), s.size_bytes() / sizeof(T) };
    }
  } // namespace detail
  
  class ViewportPointdrawTask : public detail::AbstractTask {
    // Pointset draw components
    gl::Buffer       m_point_buffer; 
    gl::Array        m_point_array;
    gl::DrawInfo     m_point_draw;
    gl::Program      m_point_program;

    // Framebuffers and attachments
    gl::Renderbuffer<float, 3,
                     gl::RenderbufferType::eMultisample>
                     m_rbuffer_msaa;
    gl::Renderbuffer<gl::DepthComponent, 1,
                     gl::RenderbufferType::eMultisample>
                     m_dbuffer_msaa;
    gl::Framebuffer  m_fbuffer_msaa;
    gl::Framebuffer  m_fbuffer;

  public:
    ViewportPointdrawTask(const std::string &name)
    : detail::AbstractTask(name) { }

    void init(detail::TaskInitInfo &info) override {
      // Obtain texture data from external resource
      auto &texture_obj = info.get_resource<io::TextureData<float>>("global", "texture_data");
      auto texture_data = detail::as_typed_span<glm::vec3>(texture_obj.data);

      // Load texture data into vertex buffer and create array object for upcoming draw
      m_point_buffer = gl::Buffer({ .data = detail::convert_span<std::byte>(texture_data) });
      m_point_array = gl::Array({ 
        .buffers = {{ .buffer = &m_point_buffer, .index = 0, .stride  = sizeof(glm::vec3) }},
        .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }}
      });

      // Build shader program
      m_point_program = gl::Program({
        { .type = gl::ShaderType::eVertex, 
          .path = "resources/shaders/viewport_task/texture_draw.vert",
          .is_spirv_binary = false },
        { .type = gl::ShaderType::eFragment,  
          .path = "resources/shaders/viewport_task/texture_draw.frag",
          .is_spirv_binary = false }
      });

      // Build draw object data for provided array object
      m_point_draw = { .type = gl::PrimitiveType::ePoints,
                       .vertex_count = (uint) texture_data.size(),
                       .bindable_array = &m_point_array,
                       .bindable_program = &m_point_program,
                       .bindable_framebuffer = &m_fbuffer_msaa };
    }

    void eval(detail::TaskEvalInfo &info) override {
      // Scoped OpenGL state
      auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA, true),
                                 gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true) };
                                 
      // Get externally shared resources 
      auto &e_viewport_texture = info.get_resource<gl::Texture2d3f>("viewport", "viewport_texture");
      auto &e_viewport_arcball = info.get_resource<detail::Arcball>("viewport", "viewport_arcball");
      auto &e_viewport_model_matrix = info.get_resource<glm::mat4>("viewport", "viewport_model_matrix");

      // (re)create framebuffers and renderbuffers if the viewport has resized
      if (!m_fbuffer.is_init() || e_viewport_texture.size() != m_rbuffer_msaa.size()) {
        m_rbuffer_msaa  = {{ .size = e_viewport_texture.size() }};
        m_dbuffer_msaa  = {{ .size = e_viewport_texture.size() }};
        m_fbuffer_msaa  = {{ .type = gl::FramebufferType::eColor, .attachment = &m_rbuffer_msaa },
                           { .type = gl::FramebufferType::eDepth, .attachment = &m_dbuffer_msaa }};
        m_fbuffer       = {{ .type = gl::FramebufferType::eColor, .attachment = &e_viewport_texture }};
      }
      
      // Push updated matrices to program
      m_point_program.uniform("model_matrix",      e_viewport_model_matrix);
      m_point_program.uniform("camera_matrix",     e_viewport_arcball.full());

      // Clear multisampled framebuffer components
      m_fbuffer_msaa.clear(gl::FramebufferType::eColor, glm::vec3(0.f));
      m_fbuffer_msaa.clear(gl::FramebufferType::eDepth, 1.f);

      // Dispatch draw call for current context
      gl::state::set_viewport(e_viewport_texture.size());
      gl::dispatch_draw(m_point_draw);

      // Blit color results into the single-sampled framebuffer with attached viewport texture
      m_fbuffer_msaa.blit_to(m_fbuffer, 
                             e_viewport_texture.size(), { 0, 0 }, 
                             e_viewport_texture.size(), { 0, 0 }, 
                             gl::FramebufferMaskFlags::eColor);
    }
  };
} // namespace met