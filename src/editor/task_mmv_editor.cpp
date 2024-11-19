#include <metameric/core/ranges.hpp>
#include <metameric/core/convex.hpp>
#include <metameric/scene/scene.hpp>
#include <metameric/editor/task_mmv_editor.hpp>
#include <metameric/editor/mmv_viewport/task_gen_mmv.hpp>
#include <metameric/editor/mmv_viewport/task_draw_mmv.hpp>
#include <metameric/editor/mmv_viewport/task_edit_mmv.hpp>
#include <metameric/editor/mmv_viewport/task_gen_patches.hpp>
#include <metameric/editor/detail/imgui.hpp>
#include <metameric/editor/detail/gizmo.hpp>
#include <metameric/editor/detail/task_arcball_input.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  struct MMVEditorBeginTask : public detail::TaskNode {
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Get shared resources
      const auto &e_cs    = info.parent()("selection").getr<ConstraintRecord>();
      const auto &e_scene = info.global("scene").getr<Scene>();
      const auto &e_vert  = e_scene.uplifting_vertex(e_cs);

      // Define window name
      auto name = fmt::format("Editing: {} (uplifting {}, vertex {})", 
        e_vert.name, e_cs.uplifting_i, e_cs.vertex_i);  
      
      // Define window size on first open
      ImGui::SetNextWindowSize({ 256, 384 }, ImGuiCond_Appearing);

      // Open main viewport window, and forward window activity to "is_active" flag
      // Note: window end is post-pended in ViewportEndTask so subtasks can do stuff with imgui state
      // Note: we track close button as an edge case
      bool is_open = true;
      bool is_active = info.parent()("is_active").getw<bool>() 
                     = ImGui::Begin(name.c_str(), &is_open);

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

  class MMVEditorImageTask : public detail::TaskNode {
    using Depthbuffer = gl::Renderbuffer<gl::DepthComponent, 1>;
    Depthbuffer m_depth_buffer;

  public:
    void resize_fb(SchedulerHandle &info, eig::Array2u size) {
      met_trace_full();

      // Get shared resources
      auto &i_frame_buffer = info("frame_buffer").getw<gl::Framebuffer>();
      auto &i_lrgb_target  = info("lrgb_target").getw<gl::Texture2d4f>();
      auto &i_srgb_target  = info("srgb_target").getw<gl::Texture2d4f>();
      
      // Recreate texture resources
      i_lrgb_target  = {{ .size = size }};
      i_srgb_target  = {{ .size = size }};
      m_depth_buffer = {{ .size = size }};

      // Recreate framebuffer, bound to newly resized resources
      i_frame_buffer = {{ .type = gl::FramebufferType::eColor, .attachment = &i_lrgb_target  },
                        { .type = gl::FramebufferType::eDepth, .attachment = &m_depth_buffer }};
    }

    bool is_active(SchedulerHandle &info) override {
      met_trace();
      
      // Get shared resources
      const auto &e_cs    = info.parent()("selection").getr<ConstraintRecord>();
      const auto &e_scene = info.global("scene").getr<Scene>();
      const auto &e_uplf  = e_scene.components.upliftings[e_cs.uplifting_i].value;
      const auto &e_vert  = e_scene.uplifting_vertex(e_cs);

      // Activate only if parent task triggers and vertex mismatching requires rendering
      return info.parent()("is_active").getr<bool>() 
          && e_vert.has_mismatching(e_scene, e_uplf);
    }

    void init(SchedulerHandle &info) override {
      met_trace();

      // Frame buffer initial state for subtasks to not "blurb" out
      info("lrgb_target").init<gl::Texture2d4f>({ .size = 1 });
      info("srgb_target").init<gl::Texture2d4f>({ .size = 1 });
      info("frame_buffer").set(gl::Framebuffer { });
    }

    void eval(SchedulerHandle &info) override {
      met_trace_full();
      
      // Get shared resources
      const auto &i_srgb_target = info("srgb_target").getr<gl::Texture2d4f>();
      
      // Declare scoped ImGui style state to remove border padding
      auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};
      
      ImGui::BeginChild("##viewport_image_view");

      // Compute viewport size s.t. texture fills rest of window
      // and if necessary resize framebuffer
      eig::Array2u image_size = static_cast<eig::Array2f>(ImGui::GetContentRegionAvail()).max(1.f).cast<uint>();
      if ((i_srgb_target.size() != image_size).any()) {
        resize_fb(info, image_size);
      }

      // Prepare framebuffer target for next subtasks
      auto &i_frame_buffer = info("frame_buffer").getw<gl::Framebuffer>();
      i_frame_buffer.bind();
      i_frame_buffer.clear(gl::FramebufferType::eColor, eig::Array4f(0, 0, 0, 0));
      i_frame_buffer.clear(gl::FramebufferType::eDepth, 1.f);

      // Place texture view using draw target
      ImGui::Image(ImGui::to_ptr(i_srgb_target.object()), 
        i_srgb_target.size().cast<float>().eval(), 
        eig::Vector2f(0, 1), eig::Vector2f(1, 0));
    }
  };

  class MMVEditorEndTask : public detail::TaskNode {
    struct UniformBuffer {
      alignas(8) eig::Array2u size;
      alignas(4) uint lrgb_to_srgb;
    };

    // Framebuffer attachments
    std::string     m_program_key;
    gl::ComputeInfo m_dispatch;
    gl::Sampler     m_sampler;
    gl::Buffer      m_uniform_buffer;
    UniformBuffer  *m_uniform_map;

  public:
    bool is_active(SchedulerHandle &info) override {
      met_trace();
      
      // Get shared resources
      const auto &e_cs    = info.parent()("selection").getr<ConstraintRecord>();
      const auto &e_scene = info.global("scene").getr<Scene>();
      const auto &e_uplf  = e_scene.components.upliftings[e_cs.uplifting_i].value;
      const auto &e_vert  = e_scene.uplifting_vertex(e_cs);

      // Activate only if parent task triggers and vertex mismatching requires rendering
      return info.parent()("is_active").getr<bool>() 
          && e_vert.has_mismatching(e_scene, e_uplf);
    }

    void init(SchedulerHandle &info) override {
      met_trace_full();
    
      // Initialize program object in cache
      std::tie(m_program_key, std::ignore) = info.global("cache").getw<gl::detail::ProgramCache>().set({{ 
        .type       = gl::ShaderType::eCompute,
        .spirv_path = "shaders/misc/texture_resample.comp.spv",
        .cross_path = "shaders/misc/texture_resample.comp.json"
      }});
      
      // NN-sampler
      m_sampler = {{ .min_filter = gl::SamplerMinFilter::eNearest, 
                     .mag_filter = gl::SamplerMagFilter::eNearest }};
      
      // Initialize uniform buffer and writeable, flushable mapping
      std::tie(m_uniform_buffer, m_uniform_map) = gl::Buffer::make_flusheable_object<UniformBuffer>();
      m_uniform_map->lrgb_to_srgb = true;
    }

    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Get handle to relative task resource
      auto image_handle = info.relative("viewport_image");
      
      // Get shared resources
      const auto &e_lrgb_target = image_handle("lrgb_target").getr<gl::Texture2d4f>();
      const auto &e_srgb_target = image_handle("srgb_target").getr<gl::Texture2d4f>();

      // Push new dispatch size, if associated textures were modified
      if (image_handle("lrgb_target").is_mutated() || is_first_eval()) {
        eig::Array2u dispatch_n    = e_lrgb_target.size();
        eig::Array2u dispatch_ndiv = ceil_div(dispatch_n, 16u);
        m_dispatch = { .groups_x = dispatch_ndiv.x(), .groups_y = dispatch_ndiv.y() };
        m_uniform_map->size = dispatch_n;
        m_uniform_buffer.flush();
      }

      // Draw relevant program from cache
      auto &program = info.global("cache").getw<gl::detail::ProgramCache>().at(m_program_key);

      // Bind image/sampler resources and program
      program.bind();
      program.bind("b_uniform", m_uniform_buffer);
      program.bind("s_image_r", m_sampler);
      program.bind("s_image_r", e_lrgb_target);
      program.bind("i_image_w", e_srgb_target);

      // Dispatch lrgb->srgb conversion
      gl::dispatch_compute(m_dispatch);
      
      // Switch back to default framebuffer
      gl::Framebuffer::make_default().bind();
      
      // Close child separator zone and finish ImGui State
      // Note: window end is post-pended here, but window begin is in ViewportBeginTask
      ImGui::EndChild();
      ImGui::End();
    }
  };

  class MMVEditorGuizmoTask : public detail::TaskNode {
    ImGui::Gizmo m_gizmo;
    Colr         m_gizmo_curr_p;
    Colr         m_gizmo_prev_p;
    
  public:
    bool is_active(SchedulerHandle &info) override {
      met_trace();

      guard(info.parent()("is_active").getr<bool>(), false);

      // Get handles, shared resources, etc
      const auto &e_scene = info.global("scene").getr<Scene>();
      const auto &e_cs    = info.parent()("selection").getr<ConstraintRecord>();
      const auto &e_uplf  = e_scene.components.upliftings[e_cs.uplifting_i].value;
      const auto &e_vert  = e_scene.uplifting_vertex(e_cs);
      
      // This task runs only if mismatching is being handled, and the mouse enters the window
      return ImGui::IsItemHovered() && e_vert.has_mismatching(e_scene, e_uplf);
    }

    void init(SchedulerHandle &info) override {
      met_trace();
      info("is_active").set(false);       // Make is_active available to detect guizmo edit
      info("closest_point").set<Colr>(0); // Expose closest point in convex hull to other tasks
      info("clip_point").set<bool>(true);
    }
  
    void eval(SchedulerHandle &info) override {
      met_trace();

      // Get handles, shared resources, etc
      const auto &e_arcball = info.relative("viewport_camera")("arcball").getr<detail::Arcball>();
      const auto &e_trnf    = info.relative("viewport_gen_mmv")("chull_trnf").getr<eig::Matrix4f>();
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &e_cs      = info.parent()("selection").getr<ConstraintRecord>();
      const auto &e_vert    = e_scene.uplifting_vertex(e_cs);
      const auto &i_clip    = info("clip_point").getr<bool>();

      // Obtain the generated convex hull for this uplifting/vertex combination
      const auto &hull = e_scene.components.upliftings.gl
                                .uplifting_data[e_cs.uplifting_i]
                                .metamer_builders[e_cs.vertex_i]
                                .hull;

      // Visitor handles gizmo and modifies color position
      e_vert.constraint | visit([&](const auto &cstr) {
        // Only continue for supported types
        using T = std::decay_t<decltype(cstr)>;
        guard_constexpr((is_nlinear_constraint<T> || is_linear_constraint<T>));
        
        // Get [0, 1] matrix and inverse, as the displayed mesh is scaled
        auto proj     = [m = e_trnf.inverse().eval()](eig::Array3f p) -> Colr { return (m * (eig::Vector4f() << p, 1).finished()).head<3>(); };
        auto proj_inv = [m = e_trnf](eig::Array3f p)                  -> Colr { return (m * (eig::Vector4f() << p, 1).finished()).head<3>(); };

        // Register gizmo start; cache current vertex position
        if (auto p  = e_vert.get_mismatch_position(), 
                 p_ = proj(p); 
                 m_gizmo.begin_delta(e_arcball, eig::Affine3f(eig::Translation3f(p_)))) {
          m_gizmo_curr_p = p;
          m_gizmo_prev_p = p;
        }

        // Register gizmo drag; apply world-space delta
        if (auto [active, delta] = m_gizmo.eval_delta(); active) {
          // Apply delta to tracked value
          m_gizmo_curr_p = proj_inv((delta * proj(m_gizmo_curr_p)));

          // Expose a marker point for the snap position inside the convex hull;
          // don't snap as it feels weird while moving the point
          auto gizmo_clip_p = info("closest_point").set<Colr>(hull.find_closest_interior(m_gizmo_curr_p))
                                                   .getr<Colr>();

          // Feed clipped color to scene
          info.global("scene").getw<Scene>()
              .uplifting_vertex(e_cs)
              .set_mismatch_position(gizmo_clip_p);

          // Tooltip shows closest clipped value
          if (ImGui::BeginTooltip()) {
            auto gizmo_srgb_p = lrgb_to_srgb(gizmo_clip_p);
            ImGui::ColorEdit3("lrgb", gizmo_clip_p.data(), ImGuiColorEditFlags_Float);
            ImGui::ColorEdit3("srgb", gizmo_srgb_p.data(), ImGuiColorEditFlags_Float);
            ImGui::EndTooltip();
          }
        }

        // Register gizmo end; apply vertex position to scene save state
        if (m_gizmo.end_delta()) {
          // Clip vertex position to inside convex hull, if enabled
          if (i_clip)
            m_gizmo_curr_p = hull.find_closest_interior(m_gizmo_curr_p);

          // Handle save
          info.global("scene").getw<Scene>().touch({
            .name = "Move color constraint",
            .redo = [p = m_gizmo_curr_p, e_cs](auto &scene) { scene.uplifting_vertex(e_cs).set_mismatch_position(p); },
            .undo = [p = m_gizmo_prev_p, e_cs](auto &scene) { scene.uplifting_vertex(e_cs).set_mismatch_position(p); }
          });
        }

        // Expose whether gizmo input is being handled for other tasks
        info("is_active").set( m_gizmo.is_active());
      });
    }
  };

  void MMVEditorTask::init(SchedulerHandle &info) {
    met_trace();

    info("is_active").set(true); // Make is_active available to detect window presence
    
    // Make selection available
    m_cs = info("selection").set(std::move(m_cs)).getr<ConstraintRecord>();

    // Spawn subtasks
    info.child_task("viewport_begin").init<MMVEditorBeginTask>();
    info.child_task("viewport_edit_mmv").init<EditMMVTask>();
    info.child_task("viewport_image").init<MMVEditorImageTask>();
    info.child_task("viewport_camera").init<detail::ArcballInputTask>(info.child("viewport_image")("lrgb_target"), 
      detail::ArcballInputTask::InfoType { .dist            = 1.f, 
                                           .e_center        = .5f, 
                                           .zoom_delta_mult = 0.025f });
    info.child_task("viewport_gen_mmv").init<GenMMVTask>();
    info.child_task("viewport_gen_patches").init<GenPatchesTask>();
    info.child_task("viewport_draw_mmv").init<DrawMMVTask>();
    info.child_task("viewport_guizmo").init<MMVEditorGuizmoTask>();
    info.child_task("viewport_end").init<MMVEditorEndTask>();
  }

  void MMVEditorTask::eval(SchedulerHandle &info) {
    met_trace();
    
    // Ensure the selected uplifting exists
    const auto &e_scene = info.global("scene").getr<Scene>();
    if (e_scene.components.upliftings.is_resized() && !is_first_eval()) {
      info("is_active").set(false);
      info.task().dstr();
      return;
    }

    // Ensure the selected constraint vertex exists
    const auto &e_uplifting = e_scene.components.upliftings[m_cs.uplifting_i];
    if (e_uplifting.state.verts.is_resized() && !is_first_eval()) {
      info("is_active").set(false);
      info.task().dstr();
      return;
    }
  }
} // namespace met