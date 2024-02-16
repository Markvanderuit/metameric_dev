#include <metameric/core/scene.hpp>
#include <metameric/components/views/task_mmv_editor.hpp>
#include <metameric/components/views/mmv_viewport/task_gen_mmv.hpp>
#include <metameric/components/views/mmv_viewport/task_draw_mmv.hpp>
#include <metameric/components/views/mmv_viewport/task_edit_mmv.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/task_arcball_input.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  // Constants
  constexpr static auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr static auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
  
  class MMVEditorBeginTask : public detail::TaskNode {
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
      auto &e_is                = info.parent()("selection").getr<InputSelection>();
      const auto &e_scene       = info.global("scene").getr<Scene>();
      const auto &e_vert        = e_scene.get_uplifting_vertex(e_is.uplifting_i, e_is.constraint_i);

      // Define window name
      auto name = std::format("Editing: {} (uplifting {}, vertex {})", 
        e_vert.name, e_is.uplifting_i, e_is.constraint_i);  
      
      // Ensure sensible window size on first open
      ImGui::SetNextWindowSize({ 256, 384 }, ImGuiCond_Appearing);

      // Open main viewport window, and forward window activity to "is_active" flag
      // Note: window end is post-pended in ViewportEndTask so subtasks can do stuff with imgui state
      // Note: we track close button as an edge case
      bool is_open = true;
      bool is_active = ImGui::Begin(name.c_str(), &is_open);
      info("is_active").getw<bool>() = is_active;

      // Close button pressed; ensure related tasks get torn down gracefully
      if (!is_open) {
        ImGui::End();
        info("is_active").getw<bool>() = false;
        info.parent_task().dstr();
        return;
      }

      // Compute viewport size s.t. texture is square
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      viewport_size.y() = viewport_size.x();
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
      i_frame_buffer.clear(gl::FramebufferType::eColor, eig::Array4f(0));
      i_frame_buffer.clear(gl::FramebufferType::eDepth, 1.f);
    }
  };

  class MMVEditorEndTask : public detail::TaskNode {
    struct UniformBuffer {
      alignas(8) eig::Array2u size;
      alignas(4) uint lrgb_to_srgb;
    };

    gl::ComputeInfo m_dispatch;
    gl::Program     m_program;
    gl::Sampler     m_sampler;
    gl::Buffer      m_uniform_buffer;
    UniformBuffer  *m_uniform_map;

    void init(SchedulerHandle &info) override {
      met_trace_full();

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

  bool MMVEditorTask::is_active(SchedulerHandle &info) {
    met_trace();
    return true;
  }

  void MMVEditorTask::init(SchedulerHandle &info) {
    met_trace();
    
    // Make selection available
    m_is = info("selection").set(std::move(m_is)).getr<InputSelection>();

    // Spawn subtasks
    info.child_task("viewport_begin").init<MMVEditorBeginTask>();
    info.child_task("viewport_edit_mmv").init<EditMMVTask>();
    info.child_task("viewport_camera_input").init<detail::ArcballInputTask>(info.child("viewport_begin")("lrgb_target"));
    info.child_task("viewport_gen_mmv").init<GenMMVTask>();
    info.child_task("viewport_draw_mmv").init<DrawMMVTask>();
    // 3. Add color controls
    // 4. Add guizmo input
    info.child_task("viewport_end").init<MMVEditorEndTask>();
  }

  void MMVEditorTask::eval(SchedulerHandle &info) {
    met_trace();
    
    // Ensure the selected uplifting exists
    const auto &e_scene = info.global("scene").getr<Scene>();
    if (e_scene.components.upliftings.is_resized()) {
      info.task().dstr();
      return;
    }

    // Ensure the selected constraint vertex exists
    const auto &e_uplifting = e_scene.components.upliftings[m_is.uplifting_i];
    if (e_uplifting.state.verts.is_resized()) {
      info.task().dstr();
      return;
    }
  }
} // namespace met