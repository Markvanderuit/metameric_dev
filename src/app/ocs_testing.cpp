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

namespace met {
  // Constants
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

  // Data objects
  Basis                          basis;
  // std::vector<eig::ArrayXf>      samples_p1;
  // std::vector<eig::ArrayXf>      samples_p0;
  // std::vector<Spec>              illuminants_p0;
  // std::vector<std::vector<Colr>> volumes_p0;

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

  // Point set draw summary object
  class PointsetDraw {
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
  };

  // Mesh draw summary object
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
  };

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
      alignas(64) eig::Matrix4f modelv_trf;
      alignas(64) eig::Matrix4f camera_trf;
      alignas(4)  float         alpha;
    };

    gl::Buffer      m_unif;
    UnifLayout     *m_unif_map;
    gl::Program     m_program;
    gl::Framebuffer m_framebuffer;

    void init(SchedulerHandle &info) override {
      met_trace_full();

      // Generate program object
      m_program = {{ .type       = gl::ShaderType::eVertex,   
                     .spirv_path = "resources/shaders/views/draw_csys.vert.spv",
                     .cross_path = "resources/shaders/views/draw_csys.vert.json" },
                   { .type       = gl::ShaderType::eFragment, 
                     .spirv_path = "resources/shaders/views/draw_csys.frag.spv",
                     .cross_path = "resources/shaders/views/draw_csys.frag.json" }};
                     
      // Generate mapped uniform buffer
      m_unif     = {{ .size = sizeof(UnifLayout), .flags = buffer_create_flags }};
      m_unif_map = m_unif.map_as<UnifLayout>(buffer_access_flags).data();
      m_unif_map->modelv_trf = eig::Matrix4f::Identity();
      m_unif_map->alpha      = 1.f;

      // Init draw info
      info("pointsets").set<std::vector<PointsetDraw>>({ /* ... */ });
      info("meshes").set<std::vector<MeshDraw>>({ /* ... */ });
    }
    
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // First, handle framebuffer allocate/resize
      if (const auto &e_target_rsrc = info("view", "target"); 
          !m_framebuffer.is_init() || e_target_rsrc.is_mutated()) {
        const auto &e_target = e_target_rsrc.getr<gl::Texture2d4f>();
        m_framebuffer = {{ .type = gl::FramebufferType::eColor, .attachment = &e_target }};
      }

      // Next, handle camera transform update
      if (const auto &e_camera_rsrc = info("view", "camera"); e_camera_rsrc.is_mutated()) {
        const auto &e_camera = e_camera_rsrc.getr<detail::Arcball>();
        m_unif_map->camera_trf = e_camera.full().matrix();
        m_unif.flush();
      }

      // Framebuffer state
      gl::state::set_viewport(info("view", "target").getr<gl::Texture2d4f>().size());
      m_framebuffer.clear(gl::FramebufferType::eColor, eig::Array4f { 0, 0, 0, 1 });
      m_framebuffer.clear(gl::FramebufferType::eDepth, 1.f);

      // Draw state
      gl::state::set_point_size(4.f);
      gl::state::set_op(gl::DepthOp::eLessOrEqual);
      gl::state::set_op(gl::CullOp::eBack);
      gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);
      auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eCullOp,    true),
                                 gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true),
                                 gl::state::ScopedSet(gl::DrawCapability::eBlendOp,   true) };
                    
      // Bind relevant resources, objects
      m_framebuffer.bind();
      m_program.bind();
      m_program.bind("b_uniform", m_unif);

      // Get draw info
      const auto &i_pointsets = info("pointsets").getr<std::vector<PointsetDraw>>();
      const auto &i_meshes    = info("meshes").getr<std::vector<MeshDraw>>();

      // Dispatch draws
      // ImGui::Begin("Draw selector");
      // ImGui::End();
      
      for (const auto &v : i_pointsets) {
        v.draw();
      }
      for (const auto &v : i_meshes) {
        v.draw();
      }

    }
  };

  struct DataTask : public detail::TaskNode {
    void init(SchedulerHandle &info) override {
      met_trace_full();
      
      // Get draw info submitters
      auto &i_pointsets = info("draw", "pointsets").getw<std::vector<PointsetDraw>>();
      auto &i_meshes    = info("draw", "meshes").getw<std::vector<MeshDraw>>();

      // Generate color system projection for rendering
      auto cs = (ColrSystem { .cmfs = models::cmfs_cie_xyz, 
                              .illuminant = models::emitter_cie_d65,
                              .n_scatters = 1 }).finalize_direct();
      
      // Generate a sampling distribution
      // UniformSampler sampler(0.f, 1.f, 4);
      // Spec cs_flat = cs.rowwise().sum().eval();
      // Distribution ds(cnt_span<const float>(cs_flat));
      
      auto samples_x = detail::gen_unit_dirs_x(256, 3);
      std::vector<Colr> samples(range_iter(samples_x));
      

      /* {
        // Generate 3d gaussian samples for rendering
        std::vector<Colr> samples(samples_x.size());
        rng::transform(samples_x, samples.begin(), [](const auto &xf) { return Colr(xf.head(3)); });
        i_pointsets.push_back(PointsetDraw(samples, "3d gaussian samples"));
      } */

      // First, weight the samples towards maximum values in the color system
      {
        rng::transform(samples, samples.begin(), [&](const Colr &sample) { 
          Spec s = (cs * sample.matrix()).eval();
          return (cs.transpose() * s.matrix()).normalized().eval();
        });
      }

      // Next, compute color system spectra
      {

        std::vector<Colr> values(samples.size());
        rng::transform(samples, values.begin(), [&](const Colr &sample) { 
          Spec s = (cs * sample.matrix()).eval();
          s = (s.matrix().normalized().array() / 2.f) + Spec(1.f);
          // for (auto &f : s)
          //   f = f >= 0.f ? 1.f : 0.f;
          return (cs.transpose() * s.matrix()).eval();
        });
        fmt::print("samples: {}\n", values);

        i_pointsets.push_back(PointsetDraw(values, "3d csys projection"));
      }
    }

    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // ...
    }
  };

  /* void init() {
    met_trace();

    // Load basis function data
    auto loaded_tree = io::load_json("resources/misc/tree.json").get<BasisTreeNode>();
    basis = loaded_tree.basis;

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

  //   std::vector<Colr> volume_p1  = generate_mismatch_boundary({
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
  //     return generate_mismatch_boundary({
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