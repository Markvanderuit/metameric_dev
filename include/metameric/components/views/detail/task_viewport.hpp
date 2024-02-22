#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met::detail {
  // Constants
  constexpr static auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr static auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
  
  class ViewportBeginMSTask : public detail::TaskNode {
    using Colorbuffer = gl::Renderbuffer<float, 4, gl::RenderbufferType::eMultisample>;
    using Depthbuffer = gl::Renderbuffer<gl::DepthComponent, 1, gl::RenderbufferType::eMultisample>;

    // Framebuffer attachments
    Colorbuffer m_color_buffer_ms;
    Depthbuffer m_depth_buffer_ms;

  public:
    void init(SchedulerHandle &info) override {
      met_trace_full();

      // Share resources for ViewportEndTask and the relevant middle tasks
      info("frame_buffer_ms").set<gl::Framebuffer>({ });
      info("frame_buffer").set<gl::Framebuffer>({ });
      info("lrgb_target").init<gl::Texture2d4f>({ .size = 1 });
      info("srgb_target").init<gl::Texture2d4f>({ .size = 1 });
      info("is_active").init<bool>(false);
    }

    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Get shared resources
      const auto &i_lrgb_target = info("lrgb_target").getr<gl::Texture2d4f>();
      const auto &i_srgb_target = info("srgb_target").getr<gl::Texture2d4f>();
      auto &i_frame_buffer_ms   = info("frame_buffer_ms").getw<gl::Framebuffer>();
      auto &i_frame_buffer      = info("frame_buffer").getw<gl::Framebuffer>();
      
      // Open main viewport window, and forward window activity to "is_active" flag
      // Note: window end is post-pended in ViewportEndTask
      bool is_active = ImGui::Begin("Mesh Viewport", 0, ImGuiWindowFlags_NoBringToFrontOnFocus);
      info("is_active").getw<bool>() = is_active;

      // Halt on window inactivity
      guard(is_active);

      // Compute viewport size minus ImGui's tab bars etc
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      if (!i_lrgb_target.is_init() || (i_lrgb_target.size() != viewport_size.cast<uint>()).any()) {
        // (Re-)create viewport texture if the viewport weas resized
        info("lrgb_target").getw<gl::Texture2d4f>() = {{ .size = viewport_size.max(1.f).cast<uint>() }};
        info("srgb_target").getw<gl::Texture2d4f>() = {{ .size = viewport_size.max(1.f).cast<uint>() }};

        // (Re-)create framebuffer and renderbuffers if the viewport has resized
        m_color_buffer_ms = {{ .size = i_lrgb_target.size().max(1).eval() }};
        m_depth_buffer_ms = {{ .size = i_lrgb_target.size().max(1).eval() }};
        i_frame_buffer_ms = {{ .type = gl::FramebufferType::eColor, .attachment = &m_color_buffer_ms },
                             { .type = gl::FramebufferType::eDepth, .attachment = &m_depth_buffer_ms }};
        i_frame_buffer    = {{ .type = gl::FramebufferType::eColor, .attachment = &i_lrgb_target }};
      }

      // Prepare framebuffer target for next subtasks
      i_frame_buffer_ms.bind();
      i_frame_buffer_ms.clear(gl::FramebufferType::eColor, eig::Array4f(0));
      i_frame_buffer_ms.clear(gl::FramebufferType::eDepth, 1.f);

      // Specify viewport for next subtasks
      gl::state::set_viewport(m_color_buffer_ms.size());    

      // Specify draw state for next subask
      gl::state::set_depth_range(0.f, 1.f);
      gl::state::set_op(gl::DepthOp::eLessOrEqual);
      gl::state::set_op(gl::CullOp::eBack);
      gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);

      // Insert image, applying viewport texture to viewport; texture can be safely drawn 
      // to later in the render loop. Flip y-axis UVs to obtain the correct orientation.
      ImGui::Image(ImGui::to_ptr(i_srgb_target.object()), viewport_size, eig::Vector2f(0, 1), eig::Vector2f(1, 0));
    }
  };

  class ViewportBeginTask : public detail::TaskNode {
    using Depthbuffer = gl::Renderbuffer<gl::DepthComponent, 1>;

    // Framebuffer attachments
    Depthbuffer m_depth_buffer;

  public:
    void init(SchedulerHandle &info) override {
      met_trace_full();

      // Share resources for ViewportEndTask and the relevant middle tasks
      info("frame_buffer").set<gl::Framebuffer>({ });
      info("lrgb_target").init<gl::Texture2d4f>({ .size = 1 });
      info("srgb_target").init<gl::Texture2d4f>({ .size = 1 });
      info("is_active").init<bool>(false);
    }

    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Get shared resources
      const auto &i_lrgb_target = info("lrgb_target").getr<gl::Texture2d4f>();
      const auto &i_srgb_target = info("srgb_target").getr<gl::Texture2d4f>();
      auto &i_frame_buffer      = info("frame_buffer").getw<gl::Framebuffer>();

      // Declare scoped ImGui style state
      auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};
      
      // Open main viewport window, and forward window activity to "is_active" flag
      // Note: window end is post-pended in ViewportEndTask
      bool is_active = ImGui::Begin("Mesh Viewport", 0, ImGuiWindowFlags_NoBringToFrontOnFocus);
      info("is_active").getw<bool>() = is_active;


      // Compute viewport size minus ImGui's tab bars etc
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      if (!i_lrgb_target.is_init() || (i_lrgb_target.size() != viewport_size.cast<uint>()).any()) {
        // (Re-)create viewport texture if the viewport weas resized
        info("lrgb_target").getw<gl::Texture2d4f>() = {{ .size = viewport_size.max(1.f).cast<uint>() }};
        info("srgb_target").getw<gl::Texture2d4f>() = {{ .size = viewport_size.max(1.f).cast<uint>() }};

        // (Re-)create framebuffer and renderbuffers if the viewport has resized
        m_depth_buffer = {{ .size = i_lrgb_target.size().max(1).eval() }};
        i_frame_buffer = {{ .type = gl::FramebufferType::eColor, .attachment = &i_lrgb_target  },
                          { .type = gl::FramebufferType::eDepth, .attachment = &m_depth_buffer }};
        i_frame_buffer.clear(gl::FramebufferType::eColor, eig::Array4f(0));
        i_frame_buffer.clear(gl::FramebufferType::eDepth, 1.f);
      }

      // Halt on window inactivity
      guard(is_active);
      
      // Prepare framebuffer target for next subtasks
      i_frame_buffer.bind();
      i_frame_buffer.clear(gl::FramebufferType::eColor, eig::Array4f(0));
      i_frame_buffer.clear(gl::FramebufferType::eDepth, 1.f);

      // Specify viewport for next subtasks
      gl::state::set_viewport(i_lrgb_target.size());    

      // Specify draw state for next subask
      gl::state::set_depth_range(0.f, 1.f);
      gl::state::set_op(gl::DepthOp::eLessOrEqual);
      gl::state::set_op(gl::CullOp::eBack);
      gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);

      // Insert image, applying viewport texture to viewport; texture can be safely drawn 
      // to later in the render loop. Flip y-axis UVs to obtain the correct orientation.
      ImGui::Image(ImGui::to_ptr(i_srgb_target.object()), viewport_size, eig::Vector2f(0, 1), eig::Vector2f(1, 0));
    }
  };

  class ViewportEndMSTask : public detail::TaskNode {
    struct UniformBuffer {
      alignas(8) eig::Array2u size;
      alignas(4) uint lrgb_to_srgb;
    };

    gl::ComputeInfo m_dispatch;
    gl::Program     m_program;
    gl::Sampler     m_sampler;
    gl::Buffer      m_uniform_buffer;
    UniformBuffer  *m_uniform_map;

  public:
    void init(SchedulerHandle &info) override {
      met_trace_full();

      // Constants
      constexpr static auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
      constexpr static auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
      
      // Set up draw components for gamma correction
      m_sampler = {{ .min_filter = gl::SamplerMinFilter::eNearest, 
                     .mag_filter = gl::SamplerMagFilter::eNearest }};
      m_program = {{ .type = gl::ShaderType::eCompute, 
                     .glsl_path  = "resources/shaders/misc/texture_resample.comp", 
                     .cross_path = "resources/shaders/misc/texture_resample.comp.json" }};
      
      // Initialize uniform buffer and writeable, flushable mapping
      m_uniform_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
      m_uniform_map    = &m_uniform_buffer.map_as<UniformBuffer>(buffer_access_flags)[0];
      m_uniform_map->lrgb_to_srgb = true;
    }
    
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Get handle to relative task resource
      auto begin_handle = info.relative("viewport_begin");

      // Finish draw operation if ViewBeginTask has an active window
      if (begin_handle("is_active").getr<bool>()) {
        // Get external resources 
        auto e_lrgb_target_handle    = begin_handle("lrgb_target");
        auto &e_framebuffer          = begin_handle("frame_buffer").getw<gl::Framebuffer>();
        const auto &e_framebuffer_ms = begin_handle("frame_buffer_ms").getr<gl::Framebuffer>();
        const auto &e_lrgb_target    = e_lrgb_target_handle.getr<gl::Texture2d4f>();

        // Blit color results into the single-sampled framebuffer with attached target draw_texture
        gl::sync::memory_barrier(gl::BarrierFlags::eFramebuffer);
        e_framebuffer_ms.blit_to(e_framebuffer, 
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
        m_program.bind("i_image_w", begin_handle("srgb_target").getw<gl::Texture2d4f>());

        // Dispatch prepared work
        gl::dispatch_compute(m_dispatch);

        // Switch back to default framebuffer
        gl::Framebuffer::make_default().bind();
      }

      // Finish ImGui state
      {
        // Declare scoped ImGui style state
        auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                             ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                             ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};

        // Note: window end is post-pended here, but window begin is in ViewportBeginTask
        ImGui::End();
      }
    }
  };

  class ViewportEndTask : public detail::TaskNode {
    struct UniformBuffer {
      alignas(8) eig::Array2u size;
      alignas(4) uint lrgb_to_srgb;
    };

    gl::ComputeInfo m_dispatch;
    gl::Program     m_program;
    gl::Sampler     m_sampler;
    gl::Buffer      m_uniform_buffer;
    UniformBuffer  *m_uniform_map;

  public:
    void init(SchedulerHandle &info) override {
      met_trace_full();

      // Constants
      constexpr static auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
      constexpr static auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
      
      // Set up draw components for gamma correction
      m_sampler = {{ .min_filter = gl::SamplerMinFilter::eNearest, 
                     .mag_filter = gl::SamplerMagFilter::eNearest }};
      m_program = {{ .type = gl::ShaderType::eCompute, 
                     .glsl_path  = "resources/shaders/misc/texture_resample.comp", 
                     .cross_path = "resources/shaders/misc/texture_resample.comp.json" }};
      
      // Initialize uniform buffer and writeable, flushable mapping
      m_uniform_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
      m_uniform_map    = &m_uniform_buffer.map_as<UniformBuffer>(buffer_access_flags)[0];
      m_uniform_map->lrgb_to_srgb = true;
    }
    
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Get handle to relative task resource
      auto begin_handle = info.relative("viewport_begin");

      // Finish draw operation if ViewBeginTask has an active window
      if (begin_handle("is_active").getr<bool>()) {
        // Get external resources 
        auto e_lrgb_target_handle = begin_handle("lrgb_target");
        const auto &e_lrgb_target = e_lrgb_target_handle.getr<gl::Texture2d4f>();

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
        m_program.bind("i_image_w", begin_handle("srgb_target").getw<gl::Texture2d4f>());

        // Dispatch prepared work
        gl::dispatch_compute(m_dispatch);

        // Switch back to default framebuffer
        gl::Framebuffer::make_default().bind();
      }

      // Finish ImGui state
      {
        // Declare scoped ImGui style state
        auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                            ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                            ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};

        // Note: window end is post-pended here, but window begin is in ViewportBeginTask
        ImGui::End();
      }
    }
  };

  // Helper object for creating viewport begin/image/end tasks
  struct ViewportTaskInfo {
    std::string  name         = "Viewport"; // Surrounding window name
    eig::Array2u size         = { -1, -1 }; // Default initial window size
    bool         is_closeable = false;      // Whether a close button appears, killing parent task on close
    bool         is_filling   = true;       // Whether imgui window filling scope is set
    bool         apply_srgb   = true;       // Whether draw output is converted in lrgb-srgb resample
  };

  class _ViewportBeginTask : public detail::TaskNode {
    ViewportTaskInfo m_info;

  public:
    _ViewportBeginTask(ViewportTaskInfo info) : m_info(info) { met_trace(); }
    
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Keep scoped ImGui state around s.t. image can fill window
      std::vector<ImGui::ScopedStyleVar> scope;
      if (m_info.is_filling)
        scope = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                  ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                  ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};
      
      // Define window size on first open
      ImGui::SetNextWindowSize(m_info.size.cast<float>().eval(), ImGuiCond_Appearing);

      // Open main viewport window, and forward window activity to "is_active" flag
      // Note: window end is post-pended in ViewportEndTask so subtasks can do stuff with imgui state
      // Note: we track close button as an edge case
      bool is_open = true;
      bool is_active = info.parent()("is_active").getw<bool>() 
                     = ImGui::Begin(m_info.name.c_str(), m_info.is_closeable ? &is_open : nullptr);
      
      // Close prematurely; subsequent tasks should not activate either way
      if (!is_active || !is_open)
        ImGui::End();
      
      // Close button pressed; ensure related tasks get torn down gracefully
      // and close ImGui scope prematurely
      if (!is_open) {
        info.parent()("is_active").set(false);
        info.parent_task().dstr();
        return; 
      }
    }
  };

  class _ViewportImageTask : public detail::TaskNode {
    using Depthbuffer = gl::Renderbuffer<gl::DepthComponent, 1>;

    // Constructor info
    ViewportTaskInfo m_info;

    // GL objects
        Depthbuffer  m_depthbuffer;
    gl::Framebuffer  m_framebuffer;

  private:
    void resize_fb(SchedulerHandle &info, eig::Array2u size) {
      met_trace_full();

      // Get shared resources
      auto &i_lrgb_target  = info("lrgb_target").getw<gl::Texture2d4f>();
      auto &i_srgb_target  = info("srgb_target").getw<gl::Texture2d4f>();
      
      // Recreate texture resources
      i_lrgb_target  = {{ .size = size }};
      i_srgb_target  = {{ .size = size }};
      m_depthbuffer = {{ .size = size }};

      // Recreate framebuffer, bound to newly resized resources
      m_framebuffer = {{ .type = gl::FramebufferType::eColor, .attachment = &i_lrgb_target },
                       { .type = gl::FramebufferType::eDepth, .attachment = &m_depthbuffer }};
    }

  public:
    _ViewportImageTask(ViewportTaskInfo info) : m_info(info) { met_trace(); }

    bool is_active(SchedulerHandle &info) override {
      met_trace();
      return info.parent()("is_active").getr<bool>();
    }

    void init(SchedulerHandle &info) override {
      met_trace();

      info("lrgb_target").init<gl::Texture2d4f>({ .size = 1 });
      info("srgb_target").init<gl::Texture2d4f>({ .size = 1 });
      
      resize_fb(info, { 1, 1 });
    }

    void eval(SchedulerHandle &info) override {
      met_trace_full();
      
      // Get shared resources
      const auto &i_srgb_target = info("srgb_target").getr<gl::Texture2d4f>();

      // Keep scoped ImGui state around s.t. image can fill window
      std::vector<ImGui::ScopedStyleVar> scope;
      if (m_info.is_filling)
        scope = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                  ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                  ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};
                          
      ImGui::BeginChild("##viewport_image_view");

      // Compute viewport size s.t. texture fills rest of window
      // and if necessary resize framebuffer
      eig::Array2u image_size = static_cast<eig::Array2f>(ImGui::GetContentRegionAvail()).max(1.f).cast<uint>();
      if ((i_srgb_target.size() != image_size).any()) {
        resize_fb(info, image_size);
      }

      // Prepare framebuffer target for potential draw tasks
      m_framebuffer.bind();
      m_framebuffer.clear(gl::FramebufferType::eColor, eig::Array4f(0, 0, 0, 0));
      m_framebuffer.clear(gl::FramebufferType::eDepth, 1.f);

      // Place texture view using draw target
      ImGui::Image(ImGui::to_ptr(i_srgb_target.object()), 
        i_srgb_target.size().cast<float>().eval(), 
        eig::Vector2f(0, 1), eig::Vector2f(1, 0));
    }
  };

  class _ViewportEndTask : public detail::TaskNode {
    struct UniformBuffer {
      alignas(8) eig::Array2u size;
      alignas(4) uint lrgb_to_srgb;
    };

    // Constructor info
    ViewportTaskInfo m_info;

    // GL objects
    gl::ComputeInfo m_dispatch;
    gl::Program     m_program;
    gl::Sampler     m_sampler;
    gl::Buffer      m_uniform_buffer;
    UniformBuffer  *m_uniform_map;

  public:
    _ViewportEndTask(ViewportTaskInfo info) : m_info(info) { met_trace(); }

    bool is_active(SchedulerHandle &info) override {
      return info.parent()("is_active").getr<bool>();
    }

    void init(SchedulerHandle &info) override {
      met_trace_full();

      // Set up draw components for gamma correction
      m_sampler = {{ .min_filter = gl::SamplerMinFilter::eNearest, 
                     .mag_filter = gl::SamplerMagFilter::eNearest }};
      m_program = {{ .type       = gl::ShaderType::eCompute, 
                     .glsl_path  = "resources/shaders/misc/texture_resample.comp", 
                     .cross_path = "resources/shaders/misc/texture_resample.comp.json" }};
      
      // Initialize uniform buffer and writeable, flushable mapping
      m_uniform_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
      m_uniform_map    = m_uniform_buffer.map_as<UniformBuffer>(buffer_access_flags).data();
      m_uniform_map->lrgb_to_srgb = true;
    }

    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Keep scoped ImGui state around s.t. image can fill window
      std::vector<ImGui::ScopedStyleVar> scope;
      if (m_info.is_filling)
        scope = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                  ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                  ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};

      if (m_info.apply_srgb) {
        // Get shared resources
        auto image_handle         = info.relative("viewport_image");
        const auto &e_lrgb_target = image_handle("lrgb_target").getr<gl::Texture2d4f>();
        const auto &e_srgb_target = image_handle("srgb_target").getr<gl::Texture2d4f>();

        // Push new dispatch size, if associated textures were modified
        if (image_handle("lrgb_target").is_mutated() || is_first_eval()) {
          eig::Array2u dispatch_n    = e_lrgb_target.size();
          eig::Array2u dispatch_ndiv = ceil_div(dispatch_n, 16u);
          m_dispatch = { .groups_x = dispatch_ndiv.x(),
                        .groups_y = dispatch_ndiv.y(),
                        .bindable_program = &m_program };
          m_uniform_map->size = dispatch_n;
          m_uniform_buffer.flush();
        }

        // Bind relevant resources for dispatch
        m_program.bind();
        m_program.bind("b_uniform", m_uniform_buffer);
        m_program.bind("s_image_r", m_sampler);
        m_program.bind("s_image_r", e_lrgb_target);
        m_program.bind("i_image_w", e_srgb_target);

        // Dispatch lrgb->srgb conversion
        gl::dispatch_compute(m_dispatch);
      }
      
      // Switch back to default framebuffer
      gl::Framebuffer::make_default().bind();
      
      // Close child separator zone and finish ImGui State
      // Note: window end is post-pended here, but window begin is in ViewportBeginTask
      ImGui::EndChild();
      ImGui::End();
    }
  };
} // namespace met::detail