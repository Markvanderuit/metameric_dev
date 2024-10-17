#pragma once

#include <metameric/core/scene.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/render/primitives_render.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <video.hpp>
#include <animation.hpp>
#include <small_gl/window.hpp>
#include <small_gl/detail/program_cache.hpp>

namespace met {
  struct ApplicationInfo {
    // Direct load scene path
    fs::path scene_path = "";

    // Path of output file
    fs::path out_path = "";
  
    // Shader cache path
    fs::path shader_path = "resources/shaders/shaders.bin";
    
    // View settings
    std::string view_name  = "FFW View";
    float       view_scale = 1.f;

    // General output settings
    uint fps = 24;         // Framerate of video
    uint spp = 4;          // Sample count per frame
    uint spp_per_step = 4; // Samples taken per render call
    
    // Start/end times of config; 0 means not enforced
    float start_time = 0.f, end_time = 0.f;
    
    // Motion data
    using KeyEvent = std::shared_ptr<anim::EventBase>;
    std::vector<KeyEvent> events;

    // Applied to fill events data for a scene context
    std::function<void(ApplicationInfo &, Scene &)> init_events;
  };

  class Application {
    using RenderType = PathRenderPrimitive;
  
    // Handles
    ResourceHandle m_scene_handle;
    ResourceHandle m_window_handle;
    
    // Objects
    ApplicationInfo m_info;
    LinearScheduler m_scheduler;
    Sensor          m_sensor;
    Image           m_image;

  private: // Private functions
    // void init_events_cube_with_dot() {
    //   met_trace();
      
    //   auto &scene = m_scene_handle.getw<Scene>();

    //   // Interpolate constraint color
    //   auto &cvert = scene.components.upliftings[0]->verts[0];
    //   anim::add_twokey<Uplifting::Vertex>({
    //     .handle = cvert,
    //     .values = { cvert.get_mismatch_position(), Colr { 0.110, 0.108, 0.105 } },
    //     .times  = { 0.f, 2.f },
    //     .fps    = m_info.fps
    //   });

    //   // Interpolate emitter position
    //   auto &emitter = scene.components.emitters[0].value;
    //   emitter.transform.position = { 96.f, 280.f, 96.f };
    //   anim::add_twokey<eig::Vector3f>({
    //     .handle = emitter.transform.position,
    //     .values = { emitter.transform.position, eig::Vector3f { 16.f, 360.f, 16.f }},
    //     .times  = { 0.f, 2.f },
    //     .fps    = m_info.fps
    //   });
    // }

    // void init_events_scene_0() {
    //   met_trace();
      
    //   auto &scene = m_scene_handle.getw<Scene>();

    //   auto &cube1 = scene.components.objects("Cube 1").value;
    //   auto &cube2 = scene.components.objects("Cube 2").value;

    //   // Move cubes, left to right
    //   anim::add_twokey<float>({
    //     .handle = cube1.transform.position[0],
    //     .values = {0.825f, -0.5f},
    //     .times  = { 2.f, 4.f },
    //     .fps    = m_info.fps
    //   });
    //   anim::add_twokey<float>({
    //     .handle = cube2.transform.position[0],
    //     .values = {0.5, -0.825f},
    //     .times  = { 2.f, 4.f },
    //     .fps    = m_info.fps
    //   });

    //   // Rotate cubes, some degrees
    //   float angle = 1.571f - (2.f - 1.571f);
    //   anim::add_twokey<float>({
    //     .handle = cube1.transform.rotation[0],
    //     .values = { 2.f, angle },
    //     .times  = { 2.f, 4.f },
    //     .fps    = m_info.fps
    //   });
    //   anim::add_twokey<float>({
    //     .handle = cube1.transform.rotation[0],
    //     .values = { 2.f, angle },
    //     .times  = { 2.f, 4.f },
    //     .fps    = m_info.fps
    //   });

    //   m_end_time = 6.f;
    // }

    // void init_events() {
    //   met_trace()

    //   auto &scene = m_scene_handle.getw<Scene>();
    //   auto &cvert = scene.components.upliftings("Default uplifting")->verts[1];

    //   anim::add_twokey<Uplifting::Vertex>({
    //     .handle = cvert,
    //     .values = { cvert.get_mismatch_position(), Colr { 0.090, 0.089, 0.068 } },
    //     .times  = { 0.f, 2.f },
    //     .fps    = m_info.fps
    //   });
      

    //   // cvert

    //   // auto &gnome = scene.components.objects("Gnome").value;
    //   // auto &emitter1 = scene.components.emitters("Emitter1").value;
    //   // auto &emitter2 = scene.components.emitters("Emitter2").value;

    //   // gnome.transform.scaling = 0.5f;

    //   /* anim::add_twokey<eig::Vector3f>({
    //     .handle = gnome.transform.position,
    //     .values = { gnome.transform.position,  (gnome.transform.position + eig::Vector3f(0.8f, 0, 0)).eval() },
    //     .times  = { .5f, 4.5f },
    //     .fps    = m_info.fps
    //   });
      
    //   anim::add_twokey<eig::Vector3f>({
    //     .handle = gnome.transform.scaling,
    //     .values = { gnome.transform.scaling, (gnome.transform.scaling + eig::Vector3f(.5f)).eval() },
    //     .times  = { .5f, 4.5f },
    //     .fps    = m_info.fps
    //   }); */
      
    //   /* // Phase between two emitters
    //   emitter1.is_active        = true;
    //   emitter2.is_active        = true;
    //   emitter1.illuminant_scale = 1;
    //   emitter2.illuminant_scale = 0; */

    //  /*  anim::add_twokey<float>({
    //     .handle = emitter1.illuminant_scale,
    //     .values = { 1.f, 0.f },
    //     .times  = { .5f, 4.5f },
    //     .fps    = m_info.fps
    //   }); */
    //   /* add_onekey<bool>({
    //     .handle = emitter1.is_active,
    //     .value  = false,
    //     .time   = 4.f
    //   }); */

    //   /* add_onekey<bool>({
    //     .handle = emitter2.is_active,
    //     .value  = true,
    //     .time   = .5f
    //   }); */
    //   /* anim::add_twokey<float>({
    //     .handle = emitter2.illuminant_scale,
    //     .values = { 0.f, 1.f },
    //     .times  = { .5f, 4.5f },
    //     .fps    = m_info.fps
    //   }); */

    //   m_start_time = 0.f;
    //   m_end_time = 5.f;
    // }

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
    // Application constructor
    Application(ApplicationInfo &&_info) 
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
        [](auto &info) { 
          met_trace();
          info.global("scene").getw<Scene>().update(); 
      });
      m_scheduler.task("gen_upliftings").init<GenUpliftingsTask>(256); // build many, not few
      m_scheduler.task("gen_objects").init<GenObjectsTask>();
      m_scheduler.task("render").init<LambdaTask>([&](auto &info) {
        met_trace();

        const auto &scene = info.global("scene").getr<Scene>();
        auto &renderer = info.global("renderer").getw<RenderType>();

        // Reset renderer internal film
        renderer.reset(m_sensor, scene);

        // Render frame over several iterations
        for (uint i = 0; i < m_info.spp; i += m_info.spp_per_step)
          renderer.render(m_sensor, scene);
        
        // Get frame data
        renderer.film().get(cast_span<float>(m_image.data()));

        // Clip HDR output
        std::for_each(std::execution::par_unseq,
          range_iter(cast_span<float>(m_image.data())),
          [](float &f) { f = std::clamp(f, 0.f, 1.f); });
      });

      // Initialize sensor from scene view
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
        
        m_sensor.film_size =( view.film_size.cast<float>() * m_info.view_scale).cast<uint>().eval();
        m_sensor.proj_trf  = arcball.proj().matrix();
        m_sensor.view_trf  = arcball.view().matrix();
        m_sensor.flush();
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
        .size       = m_sensor.film_size
      }};

      // Instantiate motions for scene animation
      m_info.init_events(m_info, m_scene_handle.getw<Scene>());
    }

    void run() {
      met_trace();

      auto &scene = m_scene_handle.getw<Scene>();
      auto &window = m_window_handle.getw<gl::Window>();

      // Begin video output
      VideoOutputStream os(m_info.out_path.string(), m_sensor.film_size, m_info.fps);
      
      for (uint frame = anim::time_to_frame(m_info.start_time, m_info.fps); ; ++frame) {
        fmt::print("\tGenerating ({}): s={}, f={}\n", m_info.scene_path.filename().string(), frame / m_info.fps, frame);

        // Evaluate motion; exit loop if no more animations are left
        guard_break(run_events(frame));

        // Perform render step
        m_scheduler.run();
          
        // Convert and write to stream
        auto rgb8 = m_image.convert({ 
          .pixel_frmt = Image::PixelFormat::eRGB,
          .pixel_type = Image::PixelType::eUChar,
          .color_frmt = Image::ColorFormat::eSRGB
        });
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

// Application entry point
int main() {
  try {
    met_trace();

    using namespace met;

    ApplicationInfo scene_0_info = {
      .scene_path   = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Fast forward/Scenes/scene_0.json",
      .out_path     = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Fast forward/Scenes/scene_0.mp4",
      .view_name    = "FFW view",
      .view_scale   = 0.25f,
      .fps          = 30u,
      .spp          = 4u,
      .spp_per_step = 4u,
      .start_time   = 0.f,
      .end_time     = 6.f,
      .init_events  = [](auto &info, Scene &scene) {
        met_trace();
        
        auto &cube1 = scene.components.objects("Cube 1").value;
        auto &cube2 = scene.components.objects("Cube 2").value;

        float move_start_time = 1.f, move_end_time = 3.5f;

        // Move cubes, left to right
        anim::add_twokey<float>(info.events, {
          .handle = cube1.transform.position[0],
          .values = {0.825f, -0.5f},
          .times  = { move_start_time, move_end_time },
          .fps    = info.fps
        });
        anim::add_twokey<float>(info.events, {
          .handle = cube2.transform.position[0],
          .values = {0.5, -0.825f},
          .times  = { move_start_time, move_end_time },
          .fps    = info.fps
        });

        // Rotate cubes, some degrees
        float angle = 1.571f - (2.f - 1.571f);
        anim::add_twokey<float>(info.events, {
          .handle = cube1.transform.rotation[0],
          .values = { 2.f, angle },
          .times  = { move_start_time, move_end_time },
          .fps    = info.fps
        });
        anim::add_twokey<float>(info.events, {
          .handle = cube2.transform.rotation[0],
          .values = { 2.f, angle },
          .times  = { move_start_time, move_end_time },
          .fps    = info.fps
        });
      }
    };

    ApplicationInfo scene_1a_info = {
      .scene_path   = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Fast forward/Scenes/scene_1a.json",
      .out_path     = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Fast forward/Scenes/scene_1a.mp4",
      .view_name    = "FFW view",
      .view_scale   = 0.5f,
      .fps          = 30u,
      .spp          = 4u,
      .spp_per_step = 4u,
      .start_time   = 0.f,
      .end_time     = 6.f,
      .init_events  = [](auto &info, Scene &scene) {
        met_trace();

        auto &cvert = scene.components.upliftings[0]->verts[0];
        auto &light = scene.components.emitters[0].value;
        
        float move_start_time = 1.f, move_end_time = 4.f;
        
        // Rotate light around
        anim::add_twokey<eig::Vector3f>(info.events,{
          .handle = light.transform.position,
          .values = { light.transform.position, eig::Vector3f { 128, 200, 128 } },
          .times  = { move_start_time, move_end_time },
          .fps    = info.fps
        });
      }
    };

    ApplicationInfo scene_1b_info = {
      .scene_path   = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Fast forward/Scenes/scene_1b.json",
      .out_path     = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Fast forward/Scenes/scene_1b.mp4",
      .view_name    = "FFW view",
      .view_scale   = 0.5f,
      .fps          = 30u,
      .spp          = 4u,
      .spp_per_step = 4u,
      .start_time   = 0.f,
      .end_time     = 6.f,
      .init_events  = [](auto &info, Scene &scene) {
        met_trace();

        auto &cvert = scene.components.upliftings[0]->verts[0];
        auto &light = scene.components.emitters[0].value;
        
        float move_start_time = 1.f, move_end_time = 3.5f;
        
        // Rotate light around
        anim::add_twokey<eig::Vector3f>(info.events,{
          .handle = light.transform.position,
          .values = { light.transform.position, eig::Vector3f { 128, 200, 128 } },
          .times  = { move_start_time, move_end_time },
          .fps    = info.fps
        });
      }
    };
    
    // Queue processes all moved info objects
    std::queue<ApplicationInfo> queue;
    queue.push(std::move(scene_0_info));
    queue.push(std::move(scene_1a_info));
    queue.push(std::move(scene_1b_info));

    // Exhaust input
    while (!queue.empty()) {
      auto task = queue.front();
      queue.pop();

      debug::check_expr(fs::exists(task.scene_path));
      fmt::print("Starting {}\n", task.scene_path.string());

      // Overwrite quality settings for consistenncy
      task.view_scale   = 1.f;
      task.spp          = 256u;
      task.spp_per_step = 4u;
      
      // Application consumes task
      Application app(std::move(task));
      app.run();
    }
  } catch (const std::exception &e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}