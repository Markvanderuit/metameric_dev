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
#include <unordered_set>
#include "nlopt.hpp"

namespace met {
  // Constants
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

  // Data objects
  Basis basis;
  gl::Program point_program;

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
    std::vector<eig::ArrayXf> gen_unit_dirs_x(uint n_samples, uint n_dims) {
      met_trace();

      std::vector<eig::ArrayXf> unit_dirs(n_samples);
      
      if (n_samples <= 128) {
        UniformSampler sampler(-1.f, 1.f, static_cast<uint>(omp_get_thread_num()));
        for (int i = 0; i < unit_dirs.size(); ++i)
          unit_dirs[i] = detail::inv_unit_sphere_cdf(sampler.next_nd(n_dims));
      } else {
        #pragma omp parallel
        {
          // Draw samples for this thread's range with separate sampler per thread
          UniformSampler sampler(-1.f, 1.f, static_cast<uint>(omp_get_thread_num()));
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

  // Point set draw summary object
 /*  class PointsetDraw {
    gl::Array   m_array;
    gl::Buffer  m_buffer;
    std::string m_name;

  public:
    PointsetDraw() = default;
    
    PointsetDraw(const std::vector<Colr> &points, std::string_view name) {
      std::vector<AlColr> copy(range_iter(points));
      m_buffer = {{ .data = cnt_span<const std::byte>(copy) }};
      m_array  = {{ .buffers = {{ .buffer = &m_buffer, .index = 0, .stride = sizeof(AlColr ) }},
                    .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }} }};
      m_name = name;
    }

    void draw() const {
      guard(m_array.is_init());
      gl::dispatch_draw({
        .type           = gl::PrimitiveType::ePoints,
        .vertex_count   = (uint) (m_buffer.size() / sizeof(AlColr)),
        .bindable_array = &m_array
      });
    }

    const std::string &name() const { return m_name; }
  }; */

  /* // Mesh draw summary object
  struct MeshDraw {
    gl::Array  m_array;
    gl::Buffer m_buffer_vt, m_buffer_el;
    std::string m_name;
    
    MeshDraw() = default;
    
    MeshDraw(const Mesh &mesh, std::string_view name) {
      AlMesh copy = convert_mesh<AlMesh>(mesh);
      m_buffer_vt = {{ .data  = cnt_span<const std::byte>(copy.verts) }};
      m_buffer_el = {{ .data  = cnt_span<const std::byte>(copy.elems) }};
      m_array  = {{ .buffers  = {{ .buffer = &m_buffer_vt, .index = 0, .stride = sizeof(AlColr ) }},
                    .attribs  = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
                    .elements = &m_buffer_el  }};
      m_name = name;
    }
    
    void draw() const {
      guard(m_array.is_init());
      gl::dispatch_draw({
        .type           = gl::PrimitiveType::eTriangles,
        .vertex_count   = (uint) (m_buffer_el.size() / sizeof(Colr)),
        .bindable_array = &m_array
      });
    }

    const std::string &name() const { return m_name; }
  }; */

  struct ViewTask : public detail::TaskNode {
    void init(SchedulerHandle &info) override {
      met_trace_full();
      info("target").init<gl::Texture2d4f>({ .size = 1 });
      info.resource("camera").init<detail::Arcball>({ 
        .dist            = 1.f,
        .e_eye           = 0.f,
        .e_center        = 1.f,
        .zoom_delta_mult = 0.1f
      });
      info("all_visible").init<bool>(true);
      info("single_visible").init<uint>(0u);
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

      /* if (ImGui::Begin("Settings")) {   
        // Get shared resources
        const auto &e_window = info.global("window").getr<gl::Window>();

        // Draw settings
        uint v_min = 0, v_max = volumes_p0.size() - 1;
        ImGui::Checkbox("All visible", &info("all_visible").getw<bool>());
        ImGui::SliderScalar("Single visible", ImGuiDataType_U32, &info("single_visible").getw<uint>(), &v_min, &v_max);

        ImGui::Separator();

        // Draw spectrum plot
        if (ImPlot::BeginPlot("Illuminant", { -1.f, 128.f * e_window.content_scale() }, ImPlotFlags_NoInputs | ImPlotFlags_NoFrame)) {
          Spec sd = illuminants_p0[info("single_visible").getw<uint>()];

          // Get wavelength values for x-axis in plot
          Spec x_values;
          for (uint i = 0; i < x_values.size(); ++i)
            x_values[i] = wavelength_at_index(i);
          Spec y_values = sd;

          // Setup minimal format for coming line plots
          ImPlot::SetupLegend(ImPlotLocation_North, ImPlotLegendFlags_Horizontal | ImPlotLegendFlags_Outside);
          ImPlot::SetupAxes("Wavelength", "##Value", ImPlotAxisFlags_NoGridLines, ImPlotAxisFlags_NoDecorations);
          ImPlot::SetupAxesLimits(wavelength_min, wavelength_max, 0.0, 1.0, ImPlotCond_Always);

          // Do the thing
          ImPlot::PlotLine("", x_values.data(), y_values.data(), wavelength_samples);

          ImPlot::EndPlot();
        }

        Spec sd = illuminants_p0[info("single_visible").getw<uint>()];
        ColrSystem csys_p1_free = { .cmfs = models::cmfs_cie_xyz, .illuminant = models::emitter_cie_ledrgb1 };
        ColrSystem csys_p1_base = { .cmfs = models::cmfs_cie_xyz, .illuminant = models::emitter_cie_fl2     };
        Colr colr_free = csys_p1_free.apply_color_direct(sd);
        Colr colr_base = csys_p1_base.apply_color_direct(sd);

        ImGui::ColorEdit3("Metamer (FL2)", colr_free.data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
        ImGui::ColorEdit3("Metamer (D65)", colr_base.data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
      }
      ImGui::End(); */
    }
  };
  
  struct DrawTask : public detail::TaskNode {
    struct UnifLayout {
      alignas(64) eig::Matrix4f matrix;
      alignas(8)  eig::Vector2f aspect;
    };

    gl::Buffer      m_unif;
    UnifLayout     *m_unif_map;
    gl::Framebuffer m_framebuffer;

    void init(SchedulerHandle &info) override {
      met_trace_full();
                     
      // Generate mapped uniform buffer
      m_unif = {{ .size = sizeof(UnifLayout), .flags = buffer_create_flags }};
      m_unif_map = m_unif.map_as<UnifLayout>(buffer_access_flags).data();
      m_unif.flush();

      // Init draw info vectors
      info("pointsets").set<std::vector<AnnotatedPointsetDraw>>({ /* ... */ });
      // info("meshes").set<std::vector<MeshDraw>>({ /* ... */ });
    }
    
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // First, handle framebuffer allocate/resize
      if (const auto &e_target_rsrc = info("view", "target"); 
          is_first_eval() || e_target_rsrc.is_mutated()) {
        const auto &e_target = e_target_rsrc.getr<gl::Texture2d4f>();
        m_framebuffer = {{ .type = gl::FramebufferType::eColor, .attachment = &e_target }};
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
      gl::state::set_op(gl::DepthOp::eLessOrEqual);
      gl::state::set_op(gl::CullOp::eBack);
      gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOne);
      // gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);
      auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eCullOp,    false),
                                 gl::state::ScopedSet(gl::DrawCapability::eDepthTest, false),
                                 gl::state::ScopedSet(gl::DrawCapability::eBlendOp,   true) };
                    
      // Bind relevant resources, objects
      m_framebuffer.bind();

      // Process point set draw tasks
      point_program.bind();
      point_program.bind("b_unif_buffer", m_unif);
      for (const auto &v : info("pointsets").getr<std::vector<AnnotatedPointsetDraw>>())
        v.draw();
    }
  };

  struct DataTask : public detail::TaskNode {
    void eval(SchedulerHandle &info) override {
      met_trace_full();
      regenerate_samples(info);
    }

    void regenerate_samples(SchedulerHandle &info) {
      met_trace_full();

      static bool show_ocs      = true;
      static bool show_mms      = true;
      static uint n_samples_ocs = 256u;
      static uint n_samples_mms = 256u;
      static float draw_alpha = 1.f;
      static float draw_size = 0.01f;
      static float z = 0.5f;

      if (ImGui::Begin("Settings")) {
        ImGui::Checkbox("Show OCS", &show_ocs);
        ImGui::Checkbox("Show MMS", &show_mms);
        
        const uint min_samples = 1u, max_samples = 16384u;
        ImGui::SliderScalar("Samples (OCS)", ImGuiDataType_U32, &n_samples_ocs, &min_samples, &max_samples);
        ImGui::SliderScalar("Samples (MMS)", ImGuiDataType_U32, &n_samples_mms, &min_samples, &max_samples);

        ImGui::SliderFloat("z", &z, 0.f, 1.f);

        ImGui::SliderFloat("draw alpha", &draw_alpha, 0.f, 1.f);
        ImGui::SliderFloat("draw size", &draw_size, 1e-3, 1.f);
      }
      ImGui::End();

      // Get draw info submitters
      auto &i_pointsets = info("draw", "pointsets").getw<std::vector<AnnotatedPointsetDraw>>();
      // auto &i_meshes    = info("draw", "meshes").getw<std::vector<MeshDraw>>();

      // Let's work in a 1D color system for now
      using CMF1 = Spec;
      using CMFT = eig::Matrix<float, wavelength_samples, 2 * CMF1::ColsAtCompileTime>;
      CMF1 cs0 =
        (((models::cmfs_cie_xyz.array().col(1) * models::emitter_cie_d65 * wavelength_ssize).eval()
        / (models::cmfs_cie_xyz.array().col(1) * models::emitter_cie_d65 * wavelength_ssize).sum())).eval();
      CMF1 cs1 =
        (((models::cmfs_cie_xyz.array().col(1) * models::emitter_cie_fl2 * wavelength_ssize).eval()
        / (models::cmfs_cie_xyz.array().col(1) * models::emitter_cie_fl2 * wavelength_ssize).sum())).eval();
      CMFT cst = (CMFT() << cs0, cs1).finished();

      // Obtain orthogonal basis through SVD of cst
      CMFT S = cst;
      eig::JacobiSVD<decltype(S)> svd;
      svd.compute(S, eig::ComputeFullV);
      auto U = (S * svd.matrixV() * svd.singularValues().asDiagonal().inverse()).eval();

      // eig::MatrixXf S(wavelength_samples, 3 + 3 * csys_i.size());


      // Generate sample unit vectors in 2d

      /* // Generate color system projection for rendering
      auto cs = (ColrSystem { .cmfs = models::cmfs_cie_xyz, 
                              .illuminant = models::emitter_cie_d65,
                              .n_scatters = 1 }).finalize_direct(); */
      // Clear render state
      i_pointsets.clear();


      // Find the borders of the combined set T = { z, z' } s.t. z in cs0 and z' in cs1
      std::vector<eig::Array2f> OCS(n_samples_ocs); // store ocs for now
      {
        std::vector<Colr> V(n_samples_ocs);
        std::vector<Spec> S(n_samples_ocs);

        // Sample unit vectors on hypersphere
        auto X2 = detail::gen_unit_dirs_x(n_samples_ocs, 2);
        std::vector<eig::Array2f> X(range_iter(X2));

        // Project samples on to color solid
        rng::transform(X, S.begin(), [&](eig::Array2f unit) { 
          return (U * unit.matrix()).eval();
        });

        // Compute optimal spectra exactly on system boundary 
        // Note; this collapsed most sampled spectra into a small boundary set
        rng::transform(S, S.begin(), [&](Spec s) { 
          for (auto &f : s)
              f = f >= 0.f ? 1.f : 0.f;
          return s;
        });

        /* 
          Given { z, z' } where z is a known color position, and z' is a set of nd-mismatches
          
         */

        /* std::unordered_set<
          Spec, 
          decltype(eig::detail::matrix_hash<float>), 
          decltype(eig::detail::matrix_equal)
        > S_unique(range_iter(S));
        S = { range_iter(S_unique) };
        fmt::print("S unique: {}\n", S.size()); */
        
        // Return color solid positions
        rng::transform(S, V.begin(), [&](Spec s) { 
          return (Colr() << cst.transpose() * s.matrix(), 0).finished();
        });

        // Copy OCS over
        rng::transform(V, OCS.begin(), [&](Colr c) {
          return (eig::Array2f() << c.x(), c.y()).finished();
        });

        // Finally, submit new draw info
        if (show_ocs)
          i_pointsets.push_back(AnnotatedPointsetDraw(V, 1e-2, { 1, 0, 0, 1 }));
      }

      /* // Given a known color of z = 0.5, find and project closest mms points
      {
        std::vector<Colr> V(n_samples_mms);

        // Minima, maxima on { z, z' }
        eig::Vector2f a0 = { z, 0 }, a1 = { z, 1 };
        
        // Find closest two interior points
        auto zsort = OCS;
        rng::sort(zsort, rng::less {}, [&](const eig::Array2f &v) {
          eig::Vector2f a = (a1 - a0).normalized();
          eig::Vector2f b = v.matrix() - a0;
          eig::Vector2f proj = a0 + a * a.dot(b);
          return (v.matrix() - proj).norm();
        });

        eig::Vector2f zmin = zsort[0];
        zsort = std::vector<eig::Array2f>(zsort.begin() + 1, zsort.begin() + 8);
        rng::sort(zsort, rng::greater {}, [&](const eig::Array2f &v) {
          return (v.matrix() - zmin).norm();
        });
        eig::Vector2f zmax = zsort[0];


        // Find minima/maxima along projected line
        // This is the mismatch volume boundary already, approximately
        eig::Vector2f a = (a1 - a0).normalized();
        float zmax_f = a.dot(zmax - a0);
        float zmin_f = a.dot(zmin - a0);
        zmax = a0 + a * zmax_f;
        zmin = a0 + a * zmin_f;
        
        V = { (Colr() << zmin, 0.f).finished(), 
              (Colr() << zmax, 0.f).finished() };

        if (show_mms)
          i_pointsets.push_back(AnnotatedPointsetDraw(V, draw_size, { 1, 1, 1, draw_alpha }));
      } */

      // Given a known color of z = 0.5, generate random z' and splat into lower dimension
      {
        std::vector<Colr>         V(n_samples_ocs);
        std::vector<eig::Array4f> colrs(n_samples_ocs);
        std::vector<float>        sizes(n_samples_ocs);

        // Minima, maxima on { z, z' }
        eig::Vector2f a0 = { z, 0 }, a1 = { z, 1 };
        eig::Vector2f a = (a1 - a0).normalized();
        
        // Find closest two interior points
        auto zsort = OCS;
        rng::sort(zsort, rng::less {}, [&](const eig::Array2f &v) {
          eig::Vector2f a = (a1 - a0).normalized();
          eig::Vector2f b = v.matrix() - a0;
          eig::Vector2f proj = a0 + a * a.dot(b);
          return (v.matrix() - proj).norm();
        });

        // Splat some points
        std::vector<eig::Array2f> splats(n_samples_ocs);
        rng::transform(OCS, splats.begin(), [&](const eig::Vector2f &v) {
          eig::Vector2f b = v - a0;
          eig::Vector2f p = a0 + a * a.dot(b);
          return p;
        });

        // Determine output colors, sizes, based on distance to line scale
        for (uint i = 0; i < n_samples_ocs; ++i) {
          eig::Vector2f v = splats[i];
          eig::Vector2f ocs = OCS[i];
          float d = (v - ocs).norm();
          float w = std::max(1.f - 10.f * d, 0.f);
          colrs[i] = { 1, 1, 1, draw_alpha * w };
          sizes[i] = draw_size; // * w;
        }

        // Output positions
        rng::transform(splats, V.begin(), [&](eig::Array2f s) { 
          return (Colr() << s, 0).finished();
        });
        /* rng::transform(OCS, colrs, [&](const eig::Vector2f &v) {
          eig::Vector2f b = v - a0;
          float dist = a.dot(b);
          // return eig::Array4f { 1, 1, 1, }
        }); */

        

        /* eig::Vector2f zmax = *rng::min_element(OCS, {}, [&](const eig::Array2f &v) {
          eig::Vector2f a = (a1 - a0).normalized();
          eig::Vector2f b = v.matrix() - a0;
          eig::Vector2f proj = a1 + a * a.dot(b);
          return (v.matrix() - proj).norm();
        }); */


        /* // Generate some points within this range
        auto X = UniformSampler(zmin_f, zmax_f, 4).next_nd(n_samples_mms);
        rng::transform(X, V.begin(), [&](float z_) { 
          return (Colr() << z, z_, 0).finished();
        }); */

        /* rng::transform(OCS, V.begin(), [&](eig::Array2f v) {
          // eig::Vector2f a = (a1 - a0).normalized();
          eig::Vector2f b = v.matrix() - a0;
          return (Colr() << a0 + a * a.dot(b), 0).finished();
        }); */

        /* // Sample random positions in [0, 1] in color system 1
        auto X_ = UniformSampler(0.f, 1.f, 4).next_nd(n_samples);
        std::vector<float> Z_(range_iter(X_));

        // Return color solid positions
        rng::transform(Z_, V.begin(), [&](float z_) { 
          return (Colr() << z, z_, 0).finished();
        }); */

        if (show_mms)
          i_pointsets.push_back(AnnotatedPointsetDraw(V, sizes, colrs));
      }

     /*  // Generate sample unit vectors
      auto X = detail::gen_unit_dirs_x(n_samples, 3);
      
      // Processed samples and resulting output colors are stored here
      std::vector<Colr> V(n_samples);
      std::vector<Spec> S(n_samples);

      // Project samples on to color system
      rng::copy(X, V.begin());
      rng::transform(V, S.begin(), [&](Colr c) { 
        return (cs * c.matrix()).eval();
      }); 

      // Next, compute optimal spectra
      rng::transform(S, S.begin(), [&](Spec s) { 
        if (toggle) {
          for (auto &f : s)
            f = f >= 0.f ? 1.f : 0.f;
        } else {
          s.matrix().normalize();
        }
        return s;
      });

      // Apply color system transform to spectra
      rng::transform(S, V.begin(), [&](Spec s) { 
        return (cs.transpose() * s.matrix()).eval();
      });  */


      // Plot window to help show some things more clearly
      if (ImGui::Begin("Plots")) {
        if (ImPlot::BeginPlot("Illuminant")) {
          // Get wavelength values for x-axis in plot
          Spec x_values;
          for (uint i = 0; i < x_values.size(); ++i)
            x_values[i] = wavelength_at_index(i);
          
          // Setup minimal format for coming line plots
          ImPlot::SetupLegend(ImPlotLocation_North, ImPlotLegendFlags_Horizontal | ImPlotLegendFlags_Outside);
          ImPlot::SetupAxes("Wavelength", "##Value", ImPlotAxisFlags_NoGridLines, ImPlotAxisFlags_NoDecorations);
          // ImPlot::SetupAxesLimits(wavelength_min, wavelength_max, 0.0, 1.0, ImPlotCond_Always);
        
          // Do the thing
          ImPlot::PlotLine("CS0 (D65)", x_values.data(), cs0.data(), wavelength_samples);
          ImPlot::PlotLine("CS1 (F11)", x_values.data(), cs1.data(), wavelength_samples);

          ImPlot::EndPlot();
        }
      }
      ImGui::End();
    }
  };

  /* void init() {
    met_trace();


    // Initialize 6D samples
    samples_p1 = detail::gen_unit_dirs_x(64, 6);
    samples_p0 = detail::gen_unit_dirs_x(256, 6);
  } */
  
  // void run() {
  //   met_trace();
      
  //   // Configurable constants
  //   Colr colr_p0 = { 0.8, 0.1, 0.1 };
  //   Colr colr_p1 = { 0.1, 0.1, 0.8 }; 
  //   Spec e_base  = models::emitter_cie_d65;
  //   Spec e       = models::emitter_cie_ledrgb1;
  //   CMFS c       = models::cmfs_cie_xyz;

  //   /* 
  //     Notation: c is sensor function, e is emitter function, p0 and p1 are path vertices
  //     with known surface colors that need uplifting.

  //     Setup is as follows:
  
  //       c <- p0 <- p1 <- e
  
  //     which means we'll observe metamerism for p0, even if (c, e) were the color system
  //     under which p0's color was "measured", due to the potential metamers in p1.
  //   */
   
  //   // First, generate a mismatch volume for p1
  //   ColrSystem csys_p1_base = { .cmfs = c, .illuminant = e_base };
  //   ColrSystem csys_p1_free = { .cmfs = c, .illuminant = e      };
  //   std::vector<CMFS> systems_i = { csys_p1_base.finalize_direct() };
  //   std::vector<Colr> signals_i = { colr_p1 };

  //   std::vector<Colr> volume_p1  = generate_mmv_boundary_colr({
  //     .basis      = basis,
  //     .systems_i  = systems_i,
  //     .signals_i  = signals_i,
  //     .system_j   = csys_p1_free.finalize_direct(),
  //     .samples    = samples_p1
  //   });

  //   // Then, for each generated sample of the volume, produce the resulting reflectance
  //   // and multiply by 'e'.
  //   illuminants_p0 = std::vector<Spec>(volume_p1.size());
  //   std::transform(std::execution::par_unseq, range_iter(volume_p1), illuminants_p0.begin(), 
  //     [&](const Colr &colr_p1_free) {
  //       auto systems = { csys_p1_base.finalize_direct(), csys_p1_free.finalize_direct() };
  //       auto signals = { colr_p1, colr_p1_free };
  //       return generate_spectrum({
  //         .basis      = basis,
  //         .systems    = systems,
  //         .signals    = signals
  //       });
  //   });

  //   // Given this new set of border "incident" radiances from p1, we can now generate mismatch
  //   // volume points for each of them and overlap these
  //   rng::transform(illuminants_p0, std::back_inserter(volumes_p0), [&](const Spec &illuminant_p0) {
  //     ColrSystem csys_p0_base = { .cmfs = c, .illuminant = illuminant_p0 * e };
  //     ColrSystem csys_p0_free = { .cmfs = c, .illuminant = e_base            };
  //     std::vector<CMFS> systems = { csys_p0_base.finalize_direct() };
  //     std::vector<Colr> signals = { colr_p0 };
  //     return generate_mmv_boundary_colr({
  //       .basis      = basis,
  //       .systems_i  = systems,
  //       .signals_i  = signals,
  //       .system_j   = csys_p0_free.finalize_direct(),
  //       .samples    = samples_p0
  //     });
  //   });
  // }

  void run() {
    met_trace();
    
    // Load basis function data
    basis = io::load_json("resources/misc/tree.json").get<BasisTreeNode>().basis;
    
    // Scheduler is responsible for handling application tasks, resources, and runtime loop
    LinearScheduler scheduler;

    // Initialize window (OpenGL context), as a resource owned by the scheduler
    auto &window = scheduler.global("window").init<gl::Window>({ 
      .size  = { 1024, 1024 }, 
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
    // met::init();
    // met::run();
    met::run();
  /* } catch (const std::exception &e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  } */
  return EXIT_SUCCESS;
}