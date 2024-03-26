#include <metameric/core/ranges.hpp>
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
  
  namespace detail {
    Colr find_closest_point_in_convex_hull(Colr p, const AlMesh &mesh) {
      guard(!mesh.verts.empty() && !mesh.elems.empty(), p);
      
      // Find triangle data
      auto tris = mesh.elems | vws::transform([&mesh](const eig::Array3u &el) {
        return std::tuple<Colr, Colr, Colr> { mesh.verts[el[0]], mesh.verts[el[1]], mesh.verts[el[2]] }; });
      
      // Project point to each triangle
      auto proj = tris | vws::transform([p](const auto &el) -> Colr {
        // Get vertices and compute edge vectors
        const auto &[a, b, c] = el;
        auto ab = (b - a).matrix().eval(), bc = (c - b).matrix().eval(), ca = (a - c).matrix().eval();
        auto ac = (c - a).matrix().eval(), ap = (p - a).matrix().eval();

        // Point lies behind face, do not project, return invalid
        auto n = ab.cross(ac).eval();
        if (n.dot((p - (a + b + c) / 3.f).matrix()) < 0.f)
          return std::numeric_limits<float>::infinity();
        
        // Compute barycentrics of point 
        float a_tri = std::abs(.5f * ac.cross(ab).norm());
        float a_ab  = std::abs(.5f * ap.cross(ab).norm());
        float a_ac  = std::abs(.5f * ac.cross(ap).norm());
        float a_bc  = std::abs(.5f * (c - p).matrix().cross((b - p).matrix()).norm());
        auto bary = (eig::Array3f(a_bc, a_ac, a_ab) / a_tri).eval();
        bary /= bary.abs().sum();

        // Recover point on triangle's plane
        Colr p_ = bary.x() * a + bary.y() * b + bary.z() * c;

        // Either clamp barycentrics to a specific edge...
        if (bary.x() < 0.f) { // bc
          float t = std::clamp((p_ - b).matrix().dot(bc) / bc.squaredNorm(), 0.f, 1.f);
          return b + t * bc.array();
        } else if (bary.y() < 0.f) { // ca
          float t = std::clamp((p_ - c).matrix().dot(ca) / ca.squaredNorm(), 0.f, 1.f);
          return c + t * ca.array();
        } else if (bary.z() < 0.f) { // ab
          float t = std::clamp((p_ - a).matrix().dot(ab) / ab.squaredNorm(), 0.f, 1.f);
          return a + t * ab.array();
        } else {
          // ... or return the actual point inside the triangle
          return p_;
        }
      });

      // Finalize only valid candidates and provide distance to each candidate
      auto candidates = proj 
                      | vws::filter([](const Colr &p_) { return !p_.isInf().all(); })
                      | vws::transform([p](const Colr &p_) -> std::pair<float, Colr> {
                          return { (p - p_).matrix().norm(), p_ }; })
                      | rng::to<std::vector>();
      
      // If a closest reprojected point was found, override current position
      if (auto it = rng::min_element(candidates, {}, &std::pair<float, Colr>::first); it != candidates.end())
        p = it->second;
      return p;
    }
  } // namespace detail

  struct MMVEditorBeginTask : public detail::TaskNode {
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Get shared resources
      const auto &e_cs    = info.parent()("selection").getr<ConstraintSelection>();
      const auto &e_scene = info.global("scene").getr<Scene>();
      const auto &e_vert  = e_scene.uplifting_vertex(e_cs);

      // Define window name
      auto name = std::format("Editing: {} (uplifting {}, vertex {})", 
        e_vert.name, e_cs.uplifting_i, e_cs.constraint_i);  
      
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
      const auto &e_cs    = info.parent()("selection").getr<ConstraintSelection>();
      const auto &e_scene = info.global("scene").getr<Scene>();
      const auto &e_vert  = e_scene.uplifting_vertex(e_cs);

      // Activate only if parent task triggers and vertex mismatching requires rendering
      return info.parent()("is_active").getr<bool>() && e_vert.has_mismatching();
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
      
      // Visual separator from editing components drawn in previous tasks
      ImGui::SeparatorText("Mismatch Volume");
      
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
      const auto &e_cs    = info.parent()("selection").getr<ConstraintSelection>();
      const auto &e_scene = info.global("scene").getr<Scene>();
      const auto &e_vert  = e_scene.uplifting_vertex(e_cs);

      // Activate only if parent task triggers and vertex mismatching requires rendering
      return info.parent()("is_active").getr<bool>() && e_vert.has_mismatching();
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
      const auto &e_cs    = info.parent()("selection").getr<ConstraintSelection>();
      const auto &e_vert  = e_scene.uplifting_vertex(e_cs);
      
      return ImGui::IsItemHovered() && e_vert.has_mismatching();
    }
  
    void eval(SchedulerHandle &info) override {
      met_trace();

      // Get handles, shared resources, etc
      const auto &e_arcball = info.relative("viewport_camera")("arcball").getr<detail::Arcball>();
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &e_cs      = info.parent()("selection").getr<ConstraintSelection>();
      const auto &e_vert    = e_scene.uplifting_vertex(e_cs);

      // Visit underlying constraints to allow guizmo editing
      std::visit(overloaded {
        [&](const ColorConstraint auto &cstr) {
          // Lambda to apply a specific vertex data to the color constraint
          auto apply_colr = [](Uplifting::Vertex &vert, Colr p) {
            std::visit(overloaded { [p](ColorConstraint auto &cstr) { 
              cstr.colr_j[0] = p;
            }, [](const auto &cstr) {}}, vert.constraint); };

          // Register gizmo start; cache current vertex position
          if (m_gizmo.begin_delta(e_arcball, eig::Affine3f(eig::Translation3f(cstr.colr_j[0]))))
            m_gizmo_prev_p = cstr.colr_j[0];

          // Register gizmo drag; apply world-space delta
          if (auto [active, delta] = m_gizmo.eval_delta(); active) {
            auto &e_vert  = info.global("scene").getw<Scene>().uplifting_vertex(e_cs);
            apply_colr(e_vert, delta * cstr.colr_j[0]);
          }

          // Register gizmo end; apply vertex position to scene save state
          if (m_gizmo.end_delta()) {
            // Snap vertex position to inside convex hull, if necessary
            const auto &e_chull = info.relative("viewport_gen_mmv")("chull").getr<AlMesh>();
            auto cstr_colr = detail::find_closest_point_in_convex_hull(cstr.colr_j[0], e_chull);

            // Handle save
            info.global("scene").getw<Scene>().touch({
              .name = "Move color constraint",
              .redo = [p = cstr_colr,      e_cs, apply_colr](auto &scene) {
                apply_colr(scene.uplifting_vertex(e_cs), p); },
              .undo = [p = m_gizmo_prev_p, e_cs, apply_colr](auto &scene) {
                apply_colr(scene.uplifting_vertex(e_cs), p); }
            });
          }
        },
        [&](const IndirectSurfaceConstraint &cstr) {
          // Lambda to apply a specific vertex data to the indirect constraint
          auto apply_colr = [](Uplifting::Vertex &vert, Colr p) {
            std::visit(overloaded { [p](IndirectSurfaceConstraint &cstr) { 
              cstr.colr = p;
            }, [](const auto &cstr) {}}, vert.constraint); };

          // Register gizmo start; cache current vertex position
          if (m_gizmo.begin_delta(e_arcball, eig::Affine3f(eig::Translation3f(cstr.colr))))
            m_gizmo_prev_p = cstr.colr;

          // Register gizmo drag; apply world-space delta
          if (auto [active, delta] = m_gizmo.eval_delta(); active) {
            auto &e_scene = info.global("scene").getw<Scene>();
            auto &e_vert  = e_scene.uplifting_vertex(e_cs);

            // Apply world-space delta; store transformed position in surface constraint
            apply_colr(e_vert, delta * cstr.colr);
          }

          // Register gizmo end; apply vertex position to scene save state
          if (m_gizmo.end_delta()) {
            // Snap vertex position to inside convex hull, if necessary
            const auto &e_chull = info.relative("viewport_gen_mmv")("chull").getr<AlMesh>();
            auto cstr_colr = detail::find_closest_point_in_convex_hull(cstr.colr, e_chull);

            // Handle save
            info.global("scene").getw<Scene>().touch({
              .name = "Move color constraint",
              .redo = [p = cstr_colr,      e_cs, apply_colr](auto &scene) { apply_colr(scene.uplifting_vertex(e_cs), p); },
              .undo = [p = m_gizmo_prev_p, e_cs, apply_colr](auto &scene) { apply_colr(scene.uplifting_vertex(e_cs), p); }
            });
          }
        },
        [](const auto &) { /* ... */ }
      }, e_vert.constraint);
    }
  };

  void MMVEditorTask::init(SchedulerHandle &info) {
    met_trace();

    // Make is_active available
    info("is_active").set(true);

    // Make selection available
    m_cs = info("selection").set(std::move(m_cs)).getr<ConstraintSelection>();

    // Spawn subtasks
    info.child_task("viewport_begin").init<MMVEditorBeginTask>();
    info.child_task("viewport_edit_mmv").init<EditMMVTask>();
    info.child_task("viewport_image").init<MMVEditorImageTask>();
    info.child_task("viewport_camera").init<detail::ArcballInputTask>(info.child("viewport_image")("lrgb_target"));
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