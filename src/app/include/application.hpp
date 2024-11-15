#pragma once

#include <metameric/core/io.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/render/primitives_render.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/window.hpp>
#include <small_gl/detail/program_cache.hpp>
#include <animation.hpp>
#include <video.hpp>

namespace met {
  struct RenderTaskInfo {
    // Direct load scene path
    fs::path scene_path = "";

    // Path of output file
    fs::path out_path = "";
  
    // Shader cache path
    fs::path shader_path = "resources/shaders/shaders.bin";
    
    // View settings
    std::string view_name  = "Default view";
    float       view_scale = 1.f;

    // General output settings
    uint fps          = 30; // Framerate of video
    uint spp          = 4;  // Sample count per frame
    uint spp_per_step = 1;  // Samples taken per render call
    
    // Start/end times of config; 0 means not enforced
    float start_time = 0.f, end_time = 0.f;
    
    // Motion data
    using KeyEvent = std::shared_ptr<anim::EventBase>;
    std::vector<KeyEvent> events;

    // Applied to fill events data for a scene context
    std::function<void(RenderTaskInfo &, Scene &)> init_events;
  };

  class RenderTask {
    using RenderType = PathRenderPrimitive;
  
    // Handles
    ResourceHandle m_scene_handle;
    ResourceHandle m_window_handle;
    
    // Objects
    RenderTaskInfo  m_info;
    LinearScheduler m_scheduler;
    Image           m_image;

  private: // Private functions
    bool run_events(uint frame) {
      // If maximum time is specified and exceeded, we kill the run; otherwise, we keep going
      bool pass_time = m_info.end_time > 0.f && anim::time_to_frame(m_info.end_time, m_info.fps) > frame;

      // Exhaust motion data; return false if no motion is active anymore
      bool pass_events = rng::fold_left(m_info.events, false, 
        [frame](bool left, auto &f) { return f->eval(frame) <= 0 || left; });
    
      // Keep running while one is true
      return pass_time || pass_events;
    }

  public:
    // RenderTask constructor
    RenderTask(RenderTaskInfo &&_info) 
    : m_info(std::move(_info)) {
      met_trace();

      // Initialize window (OpenGL context), as a resource owned by the scheduler
      m_window_handle = m_scheduler.global("window").init<gl::Window>({ 
        .swap_interval = 0, /* .flags = gl::WindowFlags::eDebug */
      });
      
      // Initialize program cache as resource ownedd by the scheduler;
      // load from file if a path is specified
      if (!m_info.shader_path.empty() && fs::exists(m_info.shader_path)) {
        m_scheduler.global("cache").set<gl::detail::ProgramCache>(m_info.shader_path);
      } else {
        m_scheduler.global("cache").set<gl::detail::ProgramCache>({ });
      }

      // Initialize scene as a resource owned by the scheduler
      m_scene_handle = m_scheduler.global("scene").set<Scene>({ /* ... */ });
      
      // Load scene data from path and push to gl
      debug::check_expr(fs::exists(m_info.scene_path));
      auto &scene = m_scene_handle.getw<Scene>();
      scene.load(m_info.scene_path);

      // We use the scheduler to ensure scene data and spectral constraints are all handled properly
      m_scheduler.task("scene_handler").init<LambdaTask>(
      [view_name = m_info.view_name, view_scale = m_info.view_scale](auto &info) { 
        met_trace();

        // Update scene data
        auto &scene = info.global("scene").getw<Scene>();
        scene.update(); 

        // Update sensor data
        const auto &[view, state] = scene.components.views(view_name);
        if (state) {
          auto &sensor = info.global("sensor").getw<Sensor>();

          eig::Affine3f trf_rot = eig::Affine3f::Identity();
          trf_rot *= eig::AngleAxisf(view.camera_trf.rotation.x(), eig::Vector3f::UnitY());
          trf_rot *= eig::AngleAxisf(view.camera_trf.rotation.y(), eig::Vector3f::UnitX());
          trf_rot *= eig::AngleAxisf(view.camera_trf.rotation.z(), eig::Vector3f::UnitZ());

          auto dir = (trf_rot * eig::Vector3f(0, 0, 1)).normalized().eval();
          auto eye = -dir; 
          auto cen = (view.camera_trf.position + dir).eval();

          detail::Arcball arcball = {{
            .fov_y    = view.camera_fov_y * std::numbers::pi_v<float> / 180.f,
            .aspect   = static_cast<float>(view.film_size.x()) / static_cast<float>(view.film_size.y()),
            .dist     = 1,
            .e_eye    = eye,
            .e_center = cen,
            .e_up     = { 0, -1, 0 } // flip for video output
          }};
          
          sensor.film_size = (view.film_size.cast<float>() * view_scale).cast<uint>().eval();
          sensor.proj_trf  = arcball.proj().matrix();
          sensor.view_trf  = arcball.view().matrix();
          sensor.flush();
        }
      });
      m_scheduler.task("gen_upliftings").init<GenUpliftingsTask>(256); // build many, not few
      m_scheduler.task("gen_objects").init<GenObjectsTask>();
      m_scheduler.task("render").init<LambdaTask>([&](auto &info) {
        met_trace();

        const auto &scene  = info.global("scene").getr<Scene>();
        const auto &sensor = info.global("sensor").getr<Sensor>();
        auto &renderer     = info.global("renderer").getw<RenderType>();

        // Reset renderer internal film
        renderer.reset(sensor, scene);

        // Render frame over several iterations
        for (uint i = 0; i < m_info.spp; i += m_info.spp_per_step)
          renderer.render(sensor, scene);
        
        // Get frame data
        renderer.film().get(cast_span<float>(m_image.data()));

        // Clip HDR output
        std::for_each(std::execution::par_unseq,
          range_iter(cast_span<float>(m_image.data())),
          [](float &f) { f = std::clamp(f, 0.f, 1.f); });
      });

      // Initialize sensor from scene view
      auto &sensor = m_scheduler.global("sensor").set<Sensor>({}).getw<Sensor>();
      {
        auto &view = scene.components.views(m_info.view_name).value;
      
        eig::Affine3f trf_rot = eig::Affine3f::Identity();
        trf_rot *= eig::AngleAxisf(view.camera_trf.rotation.x(), eig::Vector3f::UnitY());
        trf_rot *= eig::AngleAxisf(view.camera_trf.rotation.y(), eig::Vector3f::UnitX());
        trf_rot *= eig::AngleAxisf(view.camera_trf.rotation.z(), eig::Vector3f::UnitZ());

        auto dir = (trf_rot * eig::Vector3f(0, 0, 1)).normalized().eval();
        auto eye = -dir; 
        auto cen = (view.camera_trf.position + dir).eval();

        detail::Arcball arcball = {{
          .fov_y    = view.camera_fov_y * std::numbers::pi_v<float> / 180.f,
          .aspect   = static_cast<float>(view.film_size.x()) / static_cast<float>(view.film_size.y()),
          .dist     = 1,
          .e_eye    = eye,
          .e_center = cen,
          .e_up     = { 0, -1, 0 } // flip for video output
        }};
        
        sensor.film_size =( view.film_size.cast<float>() * m_info.view_scale).cast<uint>().eval();
        sensor.proj_trf  = arcball.proj().matrix();
        sensor.view_trf  = arcball.view().matrix();
        sensor.flush();
      }

      // Initialize renderer and output buffer
      m_scheduler.global("renderer").init<RenderType>({
        .spp_per_iter = m_info.spp_per_step,
        .cache_handle = m_scheduler.global("cache")
      });
      m_image = {{
        .pixel_frmt = Image::PixelFormat::eRGBA,
        .pixel_type = Image::PixelType::eFloat,
        .color_frmt = Image::ColorFormat::eLRGB,
        .size       = sensor.film_size
      }};

      // Instantiate motions for scene animation
      m_info.init_events(m_info, m_scene_handle.getw<Scene>());
    }

    void run() {
      met_trace();

      auto &scene = m_scene_handle.getw<Scene>();
      auto &window = m_window_handle.getw<gl::Window>();

      // Begin video output
      VideoOutputStream os(m_info.out_path.string(), m_image.size(), m_info.fps);
      
      for (uint frame = anim::time_to_frame(m_info.start_time, m_info.fps); ; ++frame) {
        fmt::print("Generating ({}): s={}, f={}\n",
          m_info.scene_path.filename().string(), frame / m_info.fps, frame);

        // Evaluate motion; exit loop if no more animations are left
        guard_break(run_events(frame));

        // Perform render step
        m_scheduler.run();
          
        // Convert, flip, and write to stream
        auto rgb8 = m_image.convert({ 
          .pixel_frmt = Image::PixelFormat::eRGB,
          .pixel_type = Image::PixelType::eUChar,
          .color_frmt = Image::ColorFormat::eSRGB
        }).flip(true, false);
        os.write(rgb8);

        // Handle window events every full second
        if (frame % m_info.fps == 0) {
          window.swap_buffers();
          window.poll_events();
          met_trace_frame()
        }
      }

      // End video output
      os.close();
    }
  };
} // namespace met