#include <metameric/core/ranges.hpp>
#include <metameric/core/matching.hpp>
#include <metameric/core/convex.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/components/views/task_mmv_editor.hpp>
#include <metameric/components/views/mmv_viewport/task_gen_mmv.hpp>
#include <metameric/components/views/mmv_viewport/task_draw_mmv.hpp>
#include <metameric/components/views/mmv_viewport/task_edit_mmv.hpp>
#include <metameric/components/views/mmv_viewport/task_gen_patches.hpp>
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

  struct MMVEditorBeginTask : public detail::TaskNode {
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Get shared resources
      const auto &e_cs    = info.parent()("selection").getr<ConstraintRecord>();
      const auto &e_scene = info.global("scene").getr<Scene>();
      const auto &e_vert  = e_scene.uplifting_vertex(e_cs);

      // Define window name
      auto name = std::format("Editing: {} (uplifting {}, vertex {})", 
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
      auto &e_clip              = info.relative("viewport_guizmo")("clip_point").getw<bool>();
      
      // Visual separator from editing components drawn in previous tasks
      ImGui::SeparatorText("Mismatch Volume");

      // Toggle button for clipping
      ImGui::Checkbox("Clip to volume", &e_clip);
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Metamers can only be correctly generated inside the volume");
      
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
    gl::ComputeInfo m_dispatch;
    gl::Program     m_program;
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

      // Get handle to relative task resource
      auto image_handle = info.relative("viewport_image");
      
      // Get shared resources
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
      
      // Switch back to default framebuffer
      gl::Framebuffer::make_default().bind();
      
      // Close child separator zone and finish ImGui State
      // Note: window end is post-pended here, but window begin is in ViewportBeginTask
      ImGui::EndChild();
      ImGui::End();
    }
  };

  class MMVEditorGuizmoTask : public detail::TaskNode {
    bool         m_is_gizmo_used = false;
    Colr         m_gizmo_prev_p;
    ImGui::Gizmo m_gizmo;
    
  public:
    bool is_active(SchedulerHandle &info) override {
      met_trace();

      guard(info.parent()("is_active").getr<bool>(), false);

      // Get handles, shared resources, etc
      const auto &e_scene = info.global("scene").getr<Scene>();
      const auto &e_cs    = info.parent()("selection").getr<ConstraintRecord>();
      const auto &e_uplf  = e_scene.components.upliftings[e_cs.uplifting_i].value;
      const auto &e_vert  = e_scene.uplifting_vertex(e_cs);
      
      return ImGui::IsItemHovered() 
          && e_vert.has_mismatching(e_scene, e_uplf);
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
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &e_cs      = info.parent()("selection").getr<ConstraintRecord>();
      const auto &e_vert    = e_scene.uplifting_vertex(e_cs);
      auto uplf_handle      = info.task(std::format("gen_upliftings.gen_uplifting_{}", e_cs.uplifting_i)).mask(info);
      const auto &e_hulls   = uplf_handle("mismatch_hulls").getr<std::vector<ConvexHull>>();
      const auto &e_hull    = e_hulls[e_cs.vertex_i];
      const auto &i_clip    = info("clip_point").getr<bool>();

      // Visitor to access underlying color value
      auto get_colr = [](const Uplifting::Vertex &vert) -> Colr {
        return vert.constraint | visit {
          [](const is_colr_constraint auto &cstr) { return cstr.cstr_j[0].colr_j; },
          [](const IndirectSurfaceConstraint &cstr) { return cstr.colr; },
          [](const auto &) { return Colr(0); }
        };
      };

      // Visitor to set underlying color value
      auto set_colr = [](Uplifting::Vertex &vert, Colr p) {
        vert.constraint | visit { 
          [p](is_colr_constraint auto &cstr)   { cstr.cstr_j[0].colr_j = p; }, 
          [p](IndirectSurfaceConstraint &cstr) { cstr.colr = p; },
          [](const auto &cstr) {}}; 
      };

      // Visitor handles gizmo and modifies color position
      e_vert.constraint | visit([&](const auto &cstr) {
        // Only continue for supported types
        using T = std::decay_t<decltype(cstr)>;
        guard_constexpr((std::is_same_v<T, IndirectSurfaceConstraint> || is_colr_constraint<T>));
        
        // Register gizmo start; cache current vertex position
        if (m_gizmo.begin_delta(e_arcball, eig::Affine3f(eig::Translation3f(get_colr(e_vert)))))
          m_gizmo_prev_p = get_colr(e_vert);

        // Register gizmo drag; apply world-space delta
        if (auto [active, delta] = m_gizmo.eval_delta(); active) {
          // Apply delta to color value
          auto trf_colr = (delta * get_colr(e_vert)).eval();

          // Store color and expose clamped color to other tasks
          set_colr(info.global("scene").getw<Scene>().uplifting_vertex(e_cs), trf_colr);

          // Expose a marker point for the snap position inside the convex hull;
          // don't snap as it feels weird while moving the point
          info("closest_point").set<Colr>(e_hull.find_closest_interior(trf_colr));
        }

        // Register gizmo end; apply vertex position to scene save state
        if (m_gizmo.end_delta()) {
          // Clip vertex position to inside convex hull, if enabled
          auto cstr_colr = i_clip
                         ? e_hull.find_closest_interior(get_colr(e_vert))
                         : get_colr(e_vert);

          // Handle save
          info.global("scene").getw<Scene>().touch({
            .name = "Move color constraint",
            .redo = [p = cstr_colr,      e_cs, set_colr](auto &scene) {
              set_colr(scene.uplifting_vertex(e_cs), p); },
            .undo = [p = m_gizmo_prev_p, e_cs, set_colr](auto &scene) {
              set_colr(scene.uplifting_vertex(e_cs), p); }
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
      detail::ArcballInputTask::InfoType { .dist = 1.f, .e_center = .5f, .zoom_delta_mult = 0.025f });
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