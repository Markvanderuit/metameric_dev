#include <metameric/core/data.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/task_embedding_viewport.hpp>
#include <metameric/components/views/embedding_viewport/task_draw_embedding.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  struct EmbeddingViewportViewBeginTask : public detail::TaskNode {
    void init(SchedulerHandle &info) override {
      met_trace_full();
      info("lrgb_target").init<gl::Texture2d4f>({ .size = 1 });
      info("srgb_target").init<gl::Texture2d4f>({ .size = 1 });
    }
    
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Get shared resources
      const auto &i_lrgb_target = info("lrgb_target").read_only<gl::Texture2d4f>();
      const auto &i_srgb_target = info("srgb_target").read_only<gl::Texture2d4f>();

      // Declare scoped ImGui style state
      auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};
      
      // Begin main viewport window
      ImGui::Begin("Embedding Viewport", 0, ImGuiWindowFlags_NoBringToFrontOnFocus);

      // Compute viewport size minus ImGui's tab bars etc
      // (Re-)create viewport texture if necessary; attached framebuffers are resized separately
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      if (!i_lrgb_target.is_init() || (i_lrgb_target.size() != viewport_size.cast<uint>()).any()) {
        info("lrgb_target").writeable<gl::Texture2d4f>() = {{ .size = viewport_size.max(1.f).cast<uint>() }};
        info("srgb_target").writeable<gl::Texture2d4f>() = {{ .size = viewport_size.max(1.f).cast<uint>() }};
      }

      // Insert image, applying viewport texture to viewport; texture can be safely drawn 
      // to later in the render loop. Flip y-axis UVs to obtain the correct orientation.
      ImGui::Image(ImGui::to_ptr(i_srgb_target.object()), viewport_size, eig::Vector2f(0, 1), eig::Vector2f(1, 0));

      // Note: window end is post-pended in EmbeddingViewportViewEndTask
    }
  };
  
  struct EmbeddingViewportViewEndTask : public detail::TaskNode {
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Declare scoped ImGui style state
      auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};

      // Note: window end is post-pended here, but window begin is in EmbeddingViewportViewBeginTask
      ImGui::End();
    }
  };

  class EmbeddingViewportDrawBeginTask : public detail::TaskNode {
    using Colorbuffer = gl::Renderbuffer<float, 4, gl::RenderbufferType::eMultisample>;
    using Depthbuffer = gl::Renderbuffer<gl::DepthComponent, 1, gl::RenderbufferType::eMultisample>;

    // Framebuffer attachments
    Colorbuffer m_color_buffer_ms;
    Depthbuffer m_depth_buffer_ms;
    
  public:
    void init(SchedulerHandle &info) override {
      met_trace_full();
      info.resource("frame_buffer_ms").set<gl::Framebuffer>({ });
    }

    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Get external resources 
      auto e_lrgb_target_handle = info.relative("view_begin")("lrgb_target");
      const auto &e_appl_data   = info.global("appl_data").read_only<ApplicationData>();
      const auto &e_lrgb_target = e_lrgb_target_handle.read_only<gl::Texture2d4f>();

      // Get modified resources 
      auto &i_frame_buffer_ms = info("frame_buffer_ms").writeable<gl::Framebuffer>();

      // (Re-)create framebuffer and renderbuffers if the viewport has resized
      if (!i_frame_buffer_ms.is_init() || e_lrgb_target_handle.is_mutated()) {
        m_color_buffer_ms = {{ .size = e_lrgb_target.size().max(1) }};
        m_depth_buffer_ms = {{ .size = e_lrgb_target.size().max(1) }};
        i_frame_buffer_ms = {{ .type = gl::FramebufferType::eColor, .attachment = &m_color_buffer_ms },
                             { .type = gl::FramebufferType::eDepth, .attachment = &m_depth_buffer_ms }};
      }

      eig::Array4f clear_colr = e_appl_data.color_mode == ApplicationData::ColorMode::eDark
                              ? eig::Array4f { 0, 0, 0, 1 } 
                              : ImGui::GetStyleColorVec4(ImGuiCol_ChildBg);

      // Clear framebuffer target for next subtasks
      i_frame_buffer_ms.clear(gl::FramebufferType::eColor, clear_colr);
      i_frame_buffer_ms.clear(gl::FramebufferType::eDepth, 1.f);
      i_frame_buffer_ms.bind();

      // Specify viewport for next subtasks
      gl::state::set_viewport(m_color_buffer_ms.size());    

      // Specify shared state for next tasks
      gl::state::set_depth_range(0.f, 1.f);
      gl::state::set_op(gl::DepthOp::eLess);
      gl::state::set_op(gl::CullOp::eBack);
      gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);
    }
  };

  struct EmbeddingViewportDrawEndTask : public detail::TaskNode {
    struct UniformBuffer {
      alignas(8) eig::Array2u size;
      alignas(4) uint lrgb_to_srgb;
    };

    gl::ComputeInfo m_dispatch;
    gl::Framebuffer m_frame_buffer;
    gl::Program     m_program;
    gl::Sampler     m_sampler;
    gl::Buffer      m_uniform_buffer;
    UniformBuffer  *m_uniform_map;

    void init(SchedulerHandle &info) override {
      met_trace_full();

      // Set up draw components for gamma correction
      m_sampler = {{ .min_filter = gl::SamplerMinFilter::eNearest, .mag_filter = gl::SamplerMagFilter::eNearest }};
      m_program = {{ .type = gl::ShaderType::eCompute, 
                     .glsl_path  = "resources/shaders/misc/texture_resample.comp", 
                     .cross_path = "resources/shaders/misc/texture_resample.comp.json" }};
      
      // Initialize uniform buffer and writeable, flushable mapping
      m_uniform_buffer = {{ .size = sizeof(UniformBuffer), .flags = gl::BufferCreateFlags::eMapWritePersistent }};
      m_uniform_map    = &m_uniform_buffer.map_as<UniformBuffer>(gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush)[0];
      m_uniform_map->lrgb_to_srgb = true;
    }

    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Get handles to relative task resourcess
      auto view_begin_handle = info.relative("view_begin");
      auto draw_begin_handle = info.relative("draw_begin");
      
      // Get external resources 
      auto e_lrgb_target_handle = view_begin_handle("lrgb_target");
      const auto &e_lrgb_target = e_lrgb_target_handle.read_only<gl::Texture2d4f>();

      // (Re-)create framebuffer if the viewport has resized
      if (!m_frame_buffer.is_init() || e_lrgb_target_handle.is_mutated()) {
        m_frame_buffer = {{ .type = gl::FramebufferType::eColor, .attachment = &e_lrgb_target }};
      }

      // Blit color results into the single-sampled framebuffer with attached target draw_texture
      gl::sync::memory_barrier(gl::BarrierFlags::eFramebuffer);
      draw_begin_handle("frame_buffer_ms").read_only<gl::Framebuffer>().blit_to(m_frame_buffer, 
        e_lrgb_target.size(), 0u, e_lrgb_target.size(), 0u, gl::FramebufferMaskFlags::eColor);

      // Set dispatch size correctly, if input texture size changed
      if (e_lrgb_target_handle.is_mutated()) {
        eig::Array2u dispatch_n    = e_lrgb_target.size();
        eig::Array2u dispatch_ndiv = ceil_div(dispatch_n, 16u);
        m_dispatch = { .groups_x = dispatch_ndiv.x(),
                       .groups_y = dispatch_ndiv.y(),
                       .bindable_program = &m_program };
        m_uniform_map->size = dispatch_n;
        m_uniform_buffer.flush();
      }

      // Bind image/sampler resources for coming dispatch
      m_program.bind("b_uniform", m_uniform_buffer);
      m_program.bind("s_image_r", m_sampler);
      m_program.bind("s_image_r", e_lrgb_target);
      m_program.bind("i_image_w", view_begin_handle("srgb_target").writeable<gl::Texture2d4f>());

      // Dispatch prepared work
      gl::dispatch_compute(m_dispatch);
    }
  };

  void EmbeddingViewportTask::init(SchedulerHandle &info) {
    met_trace();

    info.child_task("view_begin").init<EmbeddingViewportViewBeginTask>();
    info.child_task("view_end").init<EmbeddingViewportViewEndTask>();
    info.child_task("draw_begin").init<EmbeddingViewportDrawBeginTask>();
    info.child_task("draw_embedding").init<ViewportDrawEmbeddingTask>();
    info.child_task("draw_end").init<EmbeddingViewportDrawEndTask>();

    // Insert intermediate tasks that operate on ImGui view state
    // info.subtask("overlay").init<ViewportOverlayTask>();
    // info.subtask("input").init<ViewportInputTask>();
    // info.subtask("draw_begin").init<EmbeddingViewportDrawBeginTask>();
    // info.subtask("draw_color_system_solid").init<ViewportDrawColorSystemSolid>();
    // info.subtask("draw_meshing").init<ViewportDrawMeshingTask>();
    // info.subtask("draw_texture").init<ViewportDrawTextureTask>();
    // info.subtask("draw_bvh").init<ViewportDrawBVHTask>();
    // info.subtask("draw_end").init<EmbeddingViewportDrawEndTask>();
  }
} // namespace met