#include <metameric/core/distribution.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/tree.hpp>
#include <metameric/core/nlopt.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/misc/task_lambda.hpp>
#include <metameric/components/misc/task_frame_begin.hpp>
#include <metameric/components/misc/task_frame_end.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/window.hpp>
#include <small_gl/utility.hpp>
#include <implot.h>
#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <omp.h>
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <execution>
#include <numbers>
#include <span>
#include <unordered_set>

namespace met {
  // Constants
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

  // Data objects
  Basis basis;
  gl::Program point_program;
  gl::Program mesh_program;

  namespace detail {
    // Given a random vector in RN bounded to [-1, 1], return a vector
    // distributed over a gaussian distribution
    inline
    auto inv_gaussian_cdf(const auto &x) {
      met_trace();
      auto y = (-(x * x) + 1.f).max(.0001f).log().eval();
      auto z = (0.5f * y + (2.f / std::numbers::pi_v<float>)).eval();
      return (((z * z - y).sqrt() - z).sqrt() * x.sign()).eval();
    }
    
    // Given a random vector in RN bounded to [-1, 1], return a uniformly
    // distributed point on the unit sphere
    inline
    auto inv_unit_sphere_cdf(const auto &x) {
      met_trace();
      return inv_gaussian_cdf(x).matrix().normalized().eval();
    }

    // Generate a set of random, uniformly distributed unit vectors in RN
    inline
    std::vector<eig::ArrayXf> gen_unit_dirs_x(uint n_samples, uint n_dims, uint seed_offs = 0) {
      met_trace();

      std::vector<eig::ArrayXf> unit_dirs(n_samples);
      
      if (n_samples <= 128) {
        UniformSampler sampler(-1.f, 1.f, seed_offs);
        for (int i = 0; i < unit_dirs.size(); ++i)
          unit_dirs[i] = detail::inv_unit_sphere_cdf(sampler.next_nd(n_dims));
      } else {
        #pragma omp parallel
        {
          // Draw samples for this thread's range with separate sampler per thread
          UniformSampler sampler(-1.f, 1.f, seed_offs + static_cast<uint>(omp_get_thread_num()));
          #pragma omp for
          for (int i = 0; i < unit_dirs.size(); ++i)
            unit_dirs[i] = detail::inv_unit_sphere_cdf(sampler.next_nd(n_dims));
        }
      }

      return unit_dirs;
    }
  } // namespace detail

  class AnnotatedPointsetDraw {
    gl::Array   m_array;
    gl::Buffer  m_buffer_posi;
    gl::Buffer  m_buffer_size;
    gl::Buffer  m_buffer_colr;
    std::string m_name;

  public:
    AnnotatedPointsetDraw() = default;

    AnnotatedPointsetDraw(std::span<const Colr> posi,
                          float                 size = 1.f,
                          eig::Array4f          colr = 1.f, 
                          std::string_view      name = "") {
      std::vector<AlColr>       posi_copy(range_iter(posi));
      std::vector<float>        size_copy(posi.size(), size);
      std::vector<eig::Array4f> colr_copy(posi.size(), colr);

      m_buffer_posi = {{ .data = cnt_span<const std::byte>(posi_copy) }};
      m_buffer_size = {{ .data = cnt_span<const std::byte>(size_copy) }};
      m_buffer_colr = {{ .data = cnt_span<const std::byte>(colr_copy) }};
      
      m_array  = {{ }};
    }

    AnnotatedPointsetDraw(std::span<const Colr >        posi,
                          std::span<const float>        size,
                          std::span<const eig::Array4f> colr,
                          std::string_view              name = "") {
      guard(!posi.empty());
      
      std::vector<AlColr> posi_copy(range_iter(posi));

      m_buffer_posi = {{ .data = cnt_span<const std::byte>(posi_copy) }};
      m_buffer_size = {{ .data = cnt_span<const std::byte>(size)      }};
      m_buffer_colr = {{ .data = cnt_span<const std::byte>(colr)      }};
      
      m_array  = {{ }};
    }

    void draw() const {
      guard(m_array.is_init());
      point_program.bind("b_posi_buffer", m_buffer_posi);
      point_program.bind("b_size_buffer", m_buffer_size);
      point_program.bind("b_colr_buffer", m_buffer_colr);
      gl::dispatch_draw({
        .type             = gl::PrimitiveType::eTriangles,
        .vertex_count     = 3 * static_cast<uint>(m_buffer_posi.size() / sizeof(uint)),
        .draw_op          = gl::DrawOp::eFill,
        .bindable_array   = &m_array
      });
    }

    const std::string &name() const { return m_name; }
  };

  class AnnotatedMeshDraw {
    gl::Array  m_array;
    gl::Buffer m_buffer_vert;
    gl::Buffer m_buffer_elem;

  public:
    AnnotatedMeshDraw() = default;
    AnnotatedMeshDraw(const AlMesh &mesh,
                      float         alpha = 0.1f) {
      guard(!mesh.verts.empty() && !mesh.elems.empty());
      m_buffer_vert = {{ .data = cnt_span<const std::byte>(mesh.verts) }};
      m_buffer_elem = {{ .data = cnt_span<const std::byte>(mesh.elems) }};
      m_array = {{
        .buffers  = {{ .buffer = &m_buffer_vert, .index = 0, .stride = sizeof(eig::Array4f)   }},
        .attribs  = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
        .elements = &m_buffer_elem
      }};
    }

    void draw() const {
      guard(m_array.is_init());
      gl::dispatch_draw({
        .type             = gl::PrimitiveType::eTriangles,
        .vertex_count     = static_cast<uint>(m_buffer_elem.size() / sizeof(uint)),
        .draw_op          = gl::DrawOp::eFill,
        .bindable_array   = &m_array
      });
    }
  };

  struct ViewTask : public detail::TaskNode {
    void init(SchedulerHandle &info) override {
      met_trace_full();
      info("target").init<gl::Texture2d4f>({ .size = 1 });
      info.resource("camera").init<detail::Arcball>({ 
        .dist            = 2.0f,
        .e_eye           = 0.0f,
        .e_center        = 1.0f,
        .zoom_delta_mult = 0.1f
      });
    }
    
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Create an explicit dock space over the entire window's viewport, excluding the menu bar
      ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

      // Get shared resources
      // Declare scoped ImGui style state
      auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};

      if (ImGui::Begin("Viewport")) {
        // Handle viewport-sized texture allocation
        eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                   - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
        const auto &i_target = info("target").getr<gl::Texture2d4f>();
        if (!i_target.is_init() || (i_target.size() != viewport_size.cast<uint>()).any())
          info("target").getw<gl::Texture2d4f>() = {{ .size = viewport_size.max(1.f).cast<uint>() }};

        // Draw target to viewport as frame-filling image
        ImGui::Image(ImGui::to_ptr(i_target.object()), viewport_size, eig::Vector2f(0, 1), eig::Vector2f(1, 0));

        // Process camera input
        auto &io = ImGui::GetIO();
        if (io.MouseWheel != 0.f || io.MouseDown[1] || io.MouseDown[2]) {
          auto &i_camera = info("camera").getw<detail::Arcball>();
          i_camera.set_aspect(viewport_size.x() / viewport_size.y());
          if (io.MouseWheel != 0.f) i_camera.set_zoom_delta(-io.MouseWheel);
          if (io.MouseDown[1])      i_camera.set_ball_delta(eig::Array2f(io.MouseDelta) / viewport_size.array());
          if (io.MouseDown[2])      i_camera.set_move_delta((eig::Array3f() 
              << eig::Array2f(io.MouseDelta.x, io.MouseDelta.y) / viewport_size.array(), 0).finished());
        }
      }
      ImGui::End();
    }
  };
  
  struct DrawTask : public detail::TaskNode {
    using Depthbuffer = gl::Renderbuffer<gl::DepthComponent, 1>;

    struct UnifLayout {
      alignas(64) eig::Matrix4f matrix;
      alignas(8)  eig::Vector2f aspect;
    };

    gl::Buffer      m_unif;
    UnifLayout     *m_unif_map;
    gl::Framebuffer m_framebuffer;
    Depthbuffer     m_depthbuffer;

    void init(SchedulerHandle &info) override {
      met_trace_full();
                     
      // Generate mapped uniform buffer
      m_unif = {{ .size = sizeof(UnifLayout), .flags = buffer_create_flags }};
      m_unif_map = m_unif.map_as<UnifLayout>(buffer_access_flags).data();
      m_unif.flush();

      // Init draw info vectors
      info("pointsets").set<std::vector<AnnotatedPointsetDraw>>({ /* ... */ });
      info("meshes").set<std::vector<AnnotatedMeshDraw>>({ /* ... */ });
    }
    
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // First, handle framebuffer allocate/resize
      if (const auto &e_target_rsrc = info("view", "target"); 
          is_first_eval() || e_target_rsrc.is_mutated()) {
        const auto &e_target = e_target_rsrc.getr<gl::Texture2d4f>();
        m_depthbuffer = {{ .size = e_target.size() }};
        m_framebuffer = {{ .type = gl::FramebufferType::eColor, .attachment = &e_target      },
                         { .type = gl::FramebufferType::eDepth, .attachment = &m_depthbuffer }};
      }

      // Next, handle camera data update
      if (const auto &e_camera_rsrc = info("view", "camera"); 
          is_first_eval() || e_camera_rsrc.is_mutated()) {
        const auto &e_camera = e_camera_rsrc.getr<detail::Arcball>();
        m_unif_map->matrix = e_camera.full().matrix();
        m_unif_map->aspect = { 1.f, e_camera.aspect() };
        m_unif.flush();
      }

      // Framebuffer state
      gl::state::set_viewport(info("view", "target").getr<gl::Texture2d4f>().size());
      m_framebuffer.clear(gl::FramebufferType::eColor, eig::Array4f { 0, 0, 0, 1 });
      m_framebuffer.clear(gl::FramebufferType::eDepth, 1.f);

      // Draw state
      m_framebuffer.bind();
      gl::state::set_op(gl::DepthOp::eLessOrEqual);
      gl::state::set_op(gl::CullOp::eBack);
      
      // Process mesh set draw tasks
      {
        auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eCullOp,    true),
                                   gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true),
                                   gl::state::ScopedSet(gl::DrawCapability::eBlendOp,   true) };
        gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);
        mesh_program.bind();
        mesh_program.bind("b_camera", m_unif);
        for (const auto &v : info("meshes").getr<std::vector<AnnotatedMeshDraw>>())
          v.draw();
      }

      // Process point set draw tasks
      {
        auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eCullOp,    false),
                                   gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true),
                                   gl::state::ScopedSet(gl::DrawCapability::eBlendOp,   true) };
        gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOne);
        point_program.bind();
        point_program.bind("b_unif_buffer", m_unif);
        for (const auto &v : info("pointsets").getr<std::vector<AnnotatedPointsetDraw>>())
          v.draw();
      }
    }
  };

  struct DataTask : public detail::TaskNode {
    std::vector<Colr>              ocs_colr_set;
    std::vector<NLMMVBoundarySet>  mms_colr_sets_full;
    std::vector<NLMMVBoundarySet>  mms_colr_sets_aprx;
    std::vector<AlMesh>            mms_chulls_full;
    std::vector<AlMesh>            mms_chulls_aprx;

    // Static ImGui settings
    CMFS cs_0, cs_1, cs_2, cs_3;
    CMFS cs_v;
    Colr cv_0 = { 0.5, 0.5, 0.5 };
    Colr cv_2 = { 0.5, 0.5, 0.5 };
    uint n_scatters = 1;
    bool switch_power = false;
    bool show_ocs    = true;
    bool show_mms    = true;
    float draw_alpha = 1.f;
    float draw_size  = 0.05f;

    void init(SchedulerHandle &info) override {
      met_trace_full();

      // Define illuminant-induced mismatching to quickly generate a large metamer set
      ColrSystem csys_0 = { .cmfs = models::cmfs_cie_xyz, .illuminant = models::emitter_cie_d65, .n_scatters = 1 };
      ColrSystem csys_1 = { .cmfs = models::cmfs_cie_xyz, .illuminant = models::emitter_cie_fl11, .n_scatters = 1 };
      ColrSystem csys_2 = { .cmfs = models::cmfs_cie_xyz, .illuminant = models::emitter_cie_fl2, .n_scatters = 1 };
      ColrSystem csys_3 = { .cmfs = models::cmfs_cie_xyz, .illuminant = models::emitter_cie_ledrgb1, .n_scatters = 1 };

      // Specify color system spectra
      cs_0 = csys_0.finalize_direct();
      cs_1 = csys_1.finalize_direct();
      cs_2 = csys_2.finalize_direct();
      cs_3 = csys_3.finalize_direct();
      cs_v = cs_1; // Visualized cs

      // Generate OCS for cs_v
      {
        auto samples_ = detail::gen_unit_dirs_x(1024u, 3);
        std::vector<Colr> samples(range_iter(samples_));
        ocs_colr_set = /* nl_ */generate_ocs_boundary_colr({
          .basis   = basis,
          .system  = cs_v,
          .samples = samples,
        });
      }
    }

    void eval(SchedulerHandle &info) override {
      met_trace_full();

      {
        if (ImGui::Begin("Settings")) {
          ImGui::Checkbox("Show OCS",      &show_ocs);
          ImGui::Checkbox("Show MMS",      &show_mms);
          ImGui::SliderFloat("draw alpha", &draw_alpha, 0.f, 1.f);
          ImGui::SliderFloat("draw size",  &draw_size,  1e-3, 1.f);

          ImGui::Checkbox("Precise power solve", &switch_power);

          uint min_scatters = 1, max_scatters = 16;
          ImGui::SliderScalar("Nr. of scatters", ImGuiDataType_U32, &n_scatters, &min_scatters, &max_scatters);

          ImGui::ColorEdit3("In, cv0", cv_0.data(),  ImGuiColorEditFlags_Float);
          ImGui::ColorEdit3("In, cv2", cv_2.data(),  ImGuiColorEditFlags_Float);
        }
        ImGui::End();
      }

      {
        static uint _n_scatters = 1;
        static Colr _cv_0 = 0.5;
        static Colr _cv_2 = 0.5;
        static bool _switch_power = false;
        static uint seed = 1;
        
        if (_n_scatters != n_scatters || !_cv_0.isApprox(cv_0) || !_cv_2.isApprox(cv_2)) {
          _cv_0 = cv_0;
          _cv_2 = cv_2;
          _n_scatters = n_scatters;
          seed = 1;
          
          mms_colr_sets_full.clear();
          mms_colr_sets_aprx.clear();
          mms_chulls_full.clear();
          mms_chulls_aprx.clear();
        }

        mms_colr_sets_full.resize(n_scatters);
        mms_colr_sets_aprx.resize(n_scatters);
        mms_chulls_full.resize(n_scatters);
        mms_chulls_aprx.resize(n_scatters);

        for (uint i = 0; i < n_scatters; ++i) {
          if (i == 0) 
            seed++;
          guard_continue(seed < 256);

          // Generate points on the MMS in X for now
          auto samples   = detail::gen_unit_dirs_x(6u, 3u, seed);
          auto systems_i = { cs_0 };
          auto signals_i = { cv_0 };
          std::vector<CMFS> systems_j = { 
            // cs_0,
            cs_1, 
            // cs_2, 
            // cs_3,
          };

          // Reweight system contribution randomly
          /* UniformSampler sampler(65536 + seed);
          eig::ArrayXf sample = sampler.next_nd(systems_j.size());
          sample /= sample.sum();
          for (uint i = 0; i < systems_j.size(); ++i) {
            auto &cs = systems_j[i];
            cs = cs.array() * sample[i];
          } */

          // Generate points on the mms convex hull
          auto samples_  = detail::gen_unit_dirs_x(6u, 3u, seed);
          /* mms_colr_sets_full[i].insert_range(nl_generate_mmv_boundary_colr({
            .basis     = basis,
            .systems_i = systems_i,
            .signals_i = signals_i,
            .system_j  = cs_1,
            .samples   = samples_,
            .system_j_override = cs_v // TODO remove this absolute hack
          })); */
          mms_colr_sets_full[i].insert_range(nl_generate_mmv_boundary_colr({
            .basis     = basis,
            .systems_i = systems_i,
            .signals_i = signals_i,
            .systems_j = systems_j,
            .system_j  = cs_v,
            .samples   = samples
          }, static_cast<double>(i + 1), true));
          mms_colr_sets_aprx[i].insert_range(nl_generate_mmv_boundary_colr({
            .basis     = basis,
            .systems_i = systems_i,
            .signals_i = signals_i,
            .systems_j = systems_j,
            .system_j  = cs_v,
            .samples   = samples
          }, static_cast<double>(i + 1), false));

          // Determine extents of generated point sets
          auto full_max = rng::fold_left_first(mms_colr_sets_full[i], 
            [](const auto &a, const auto &b) { return a.max(b).eval(); }).value();
          auto full_min = rng::fold_left_first(mms_colr_sets_full[i], 
            [](const auto &a, const auto &b) { return a.min(b).eval(); }).value();
          auto aprx_max = rng::fold_left_first(mms_colr_sets_aprx[i], 
            [](const auto &a, const auto &b) { return a.max(b).eval(); }).value();
          auto aprx_min = rng::fold_left_first(mms_colr_sets_aprx[i], 
            [](const auto &a, const auto &b) { return a.min(b).eval(); }).value();

          // Generate corresponding convex hulls, if the minimum nr. of points is available
          // and the shape is large enough for qhull to not break the application
          if (mms_colr_sets_full[i].size() >= 4 && (full_max - full_min).minCoeff() >= 0.005f) {
            std::vector<Colr> span(range_iter(mms_colr_sets_full[i]));
            mms_chulls_full[i] = generate_convex_hull<AlMesh, Colr>(span);
          }
          if (mms_colr_sets_aprx[i].size() >= 4 && (aprx_max - aprx_min).minCoeff() >= 0.005f) {
            std::vector<Colr> span(range_iter(mms_colr_sets_aprx[i]));
            mms_chulls_aprx[i] = generate_convex_hull<AlMesh, Colr>(span);
          }
        }
      }

      regenerate_samples(info);
    }

    void regenerate_samples(SchedulerHandle &info) {
      met_trace_full();

      // Get draw info submitters
      auto &i_pointsets = info("draw", "pointsets").getw<std::vector<AnnotatedPointsetDraw>>();
      auto &i_meshes    = info("draw", "meshes").getw<std::vector<AnnotatedMeshDraw>>();

      // Clear render state
      i_pointsets.clear();
      i_meshes.clear();

      // Make OCS available for rendering
      if (show_ocs) {
        std::vector<eig::Array4f> colrs(ocs_colr_set.size());
        std::vector<float>        sizes(ocs_colr_set.size(), draw_size);
        rng::transform(ocs_colr_set, colrs.begin(), [&](const Colr &c) {
          return (eig::Array4f() << c, draw_alpha).finished();
        });
        i_pointsets.push_back(AnnotatedPointsetDraw(ocs_colr_set, sizes, colrs));
      }

      // Make MMV available for rendering
      if (show_mms) {
        for (uint i = 0; i < n_scatters; ++i) {
          // std::vector<eig::Array4f> colrs(mms_colr_sets_full[i].size());
          // std::vector<float>        sizes(mms_colr_sets_full[i].size(), 0.5 * draw_size);
          // rng::transform(mms_colr_sets[i], colrs.begin(), [&](const Colr &c) {
          //   return (eig::Array4f() << c, draw_alpha).finished();
          // });
          // i_pointsets.push_back(AnnotatedPointsetDraw(mms_colr_sets[i], sizes, colrs));

          if (switch_power) {
            if (mms_chulls_full[i].verts.empty()) {
              std::vector<Colr> span(range_iter(mms_colr_sets_full[i]));
              i_pointsets.push_back(AnnotatedPointsetDraw(span, draw_size));
            } else {
              i_meshes.push_back(AnnotatedMeshDraw(mms_chulls_full[i]));
            }
          } else {
            if (mms_chulls_aprx[i].verts.empty()) {
              std::vector<Colr> span(range_iter(mms_colr_sets_aprx[i]));
              i_pointsets.push_back(AnnotatedPointsetDraw(span, draw_size));
            } else {
              i_meshes.push_back(AnnotatedMeshDraw(mms_chulls_aprx[i]));
            }
          }
        }
      }
    }
  };

  void run() {
    met_trace();
    
    // Load basis function data
    basis = io::load_json("resources/misc/tree.json").get<BasisTreeNode>().basis;
    
    // Scheduler is responsible for handling application tasks, resources, and runtime loop
    LinearScheduler scheduler;

    // Initialize window (OpenGL context), as a resource owned by the scheduler
    auto &window = scheduler.global("window").init<gl::Window>({ 
      .size  = { 1280, 1024 }, 
      .title = "Mismatch testing", 
      .flags = gl::WindowCreateFlags::eVisible   | gl::WindowCreateFlags::eFocused 
             | gl::WindowCreateFlags::eDecorated | gl::WindowCreateFlags::eResizable 
             | gl::WindowCreateFlags::eMSAA met_debug_insert(| gl::WindowCreateFlags::eDebug)
    }).getw<gl::Window>();

    // Initialize OpenGL debug messages, if requested
    if constexpr (met_enable_debug) {
      gl::debug::enable_messages(gl::DebugMessageSeverity::eLow, gl::DebugMessageTypeFlags::eAll);
      gl::debug::insert_message("OpenGL debug messages are active!", gl::DebugMessageSeverity::eLow);
    }
    
    // Generate program objects
    point_program = {{ .type       = gl::ShaderType::eVertex,   
                       .spirv_path = "resources/shaders/views/ocs_test_draw.vert.spv",
                       .cross_path = "resources/shaders/views/ocs_test_draw.vert.json" },
                     { .type       = gl::ShaderType::eFragment, 
                       .spirv_path = "resources/shaders/views/ocs_test_draw.frag.spv",
                       .cross_path = "resources/shaders/views/ocs_test_draw.frag.json" }};
    mesh_program = {{ .type       = gl::ShaderType::eVertex,   
                      .spirv_path = "resources/shaders/views/draw_meshing_elem.vert.spv",
                      .cross_path = "resources/shaders/views/draw_meshing_elem.vert.json" },
                    { .type       = gl::ShaderType::eFragment, 
                      .spirv_path = "resources/shaders/views/draw_meshing_elem.frag.spv",
                      .cross_path = "resources/shaders/views/draw_meshing_elem.frag.json" }};
    

    // Create and start runtime loop
    scheduler.task("frame_begin").init<FrameBeginTask>();
    scheduler.task("view").init<ViewTask>();
    scheduler.task("draw").init<DrawTask>();
    scheduler.task("data").init<DataTask>();
    scheduler.task("frame_end").init<FrameEndTask>(true);

    while (!window.should_close())
      scheduler.run();
  }
} // namespace met

int main() {
  /* try { */
    met::run();
  /* } catch (const std::exception &e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  } */
  return EXIT_SUCCESS;
}