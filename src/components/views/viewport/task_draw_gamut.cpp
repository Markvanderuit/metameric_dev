#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/viewport/task_draw_gamut.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  constexpr uint  max_elements    = 16u;
  constexpr float deselected_opac = 0.05f;
  constexpr float mouseover_opac  = 0.35f;
  constexpr float selected_opac   = 0.25f;

  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

  ViewportDrawGamutTask::ViewportDrawGamutTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportDrawGamutTask::init(detail::TaskInitInfo &info) {
    met_trace_full();
    
    // Get shared resources
    auto &e_gamut_vert = info.get_resource<gl::Buffer>("gen_spectral_gamut", "colr_buffer");
    auto &e_gamut_elem = info.get_resource<gl::Buffer>("gen_spectral_gamut", "elem_buffer_unal");

    // Setup program for colored element draw
    m_program = {{ .type = gl::ShaderType::eVertex, .path = "resources/shaders/viewport/draw_gamut.vert" },
                 { .type = gl::ShaderType::eFragment, .path = "resources/shaders/viewport/draw_gamut.frag" }};

    // Setup opacity buffer using mapping flags
    std::vector<float> input_sizes(max_elements, deselected_opac);
    m_opac_buffer = {{ .data = cnt_span<const std::byte>(input_sizes), .flags = buffer_create_flags }};
    m_opac_map = cast_span<float>(m_opac_buffer.map(buffer_access_flags));

    // Setup objects for gamut line/triangle draw
    m_array =  {{ .buffers = {{ .buffer = &e_gamut_vert,  .index = 0, .stride = sizeof(eig::AlArray3f) }},
                  .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
                  .elements = &e_gamut_elem }};
    m_draw = { .type             = gl::PrimitiveType::eTriangles,
               .vertex_count     = static_cast<uint>(e_gamut_elem.size() / sizeof(uint)),
               .bindable_array   = &m_array,
               .bindable_program = &m_program };

    // Set non-changing uniform values
    m_program.uniform("u_model_matrix", eig::Matrix4f::Identity().eval());
    m_program.uniform("u_offset",       .5f);

    m_gamut_vert_cache = e_gamut_vert.object();
  }

  void ViewportDrawGamutTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();

    m_opac_buffer.unmap();
  }


  void ViewportDrawGamutTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();
                                
    // Get shared resources 
    auto &e_viewport_arcball = info.get_resource<detail::Arcball>("viewport_input", "arcball");
    auto &e_gamut_vert       = info.get_resource<gl::Buffer>("gen_spectral_gamut", "colr_buffer");
    auto &e_gamut_elem       = info.get_resource<gl::Buffer>("gen_spectral_gamut", "elem_buffer_unal");
    auto &e_gamut_selection  = info.get_resource<std::vector<uint>>("viewport_input_elem", "selection");
    auto &e_gamut_mouseover = info.get_resource<std::vector<uint>>("viewport_input_elem", "mouseover");

    // Update array object in case gamut buffer was resized
    if (m_gamut_vert_cache != e_gamut_vert.object()) {
      m_gamut_vert_cache = e_gamut_vert.object();
      m_array.attach_buffer({{ .buffer = &e_gamut_vert, .index = 0, .stride = sizeof(eig::AlArray3f) }});
      m_array.attach_elements(e_gamut_elem);
      m_draw.vertex_count = static_cast<uint>(e_gamut_elem.size() / sizeof(uint));
    }

    // On state change, update draw opacity based on selected triangles
    if (!std::ranges::equal(m_gamut_select_cache, e_gamut_selection) || !std::ranges::equal(m_gamut_msover_cache, e_gamut_mouseover)) {
      std::ranges::fill(m_opac_map, deselected_opac);
      std::ranges::for_each(e_gamut_mouseover, [&](uint i) { m_opac_map[i] = mouseover_opac; });
      std::ranges::for_each(e_gamut_selection, [&](uint i) { m_opac_map[i] = selected_opac; });
      m_opac_buffer.flush();
      m_gamut_select_cache = e_gamut_selection;
      m_gamut_msover_cache = e_gamut_mouseover;
    }
    
    // Update program uniforms
    m_program.uniform("u_camera_matrix", e_viewport_arcball.full().matrix());

    // Fixed state specifiers
    gl::state::set_op(gl::CullOp::eFront);
    gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);

    // Handle base line draw
    {
      // Declare scoped OpenGL state
      auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,      true),
                                 gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true) };
                                 
      // Dispatch draws for gamut lines
      m_program.uniform("u_use_opacity", false);
      gl::state::set_op(gl::DrawOp::eLine);
      gl::dispatch_draw(m_draw);
    }

    // Handle selected face draw
    {
      // Declare scoped OpenGL state
      auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,      true),
                                 gl::state::ScopedSet(gl::DrawCapability::eDepthTest, false),
                                 gl::state::ScopedSet(gl::DrawCapability::eCullOp,    false),
                                 gl::state::ScopedSet(gl::DrawCapability::eBlendOp,   true) };

      // Bind resource to buffer target and set uniform flag to enable opacity
      m_opac_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0u);
      m_program.uniform("u_use_opacity", true);
      
      // Dispatch draws for gamut selection
      gl::state::set_op(gl::DrawOp::eFill);
      gl::dispatch_draw(m_draw);
    }
  }
} // namespace met