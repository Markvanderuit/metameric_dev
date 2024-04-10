#include <metameric/core/scene.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/schedule.hpp>
#include <small_gl/window.hpp>
#include <small_gl/program.hpp>
#include <cstdlib>
#include <exception>

namespace met {
  // Application create settings
  struct MetamericEditorInfo {
    // Direct load scene
    fs::path scene_path = "";

    // Window settings
    eig::Array2u app_size  = { 1680, 1024 };
    std::string  app_title = "Metameric Editor";
  };

  // Application create function
  void create_metameric_editor(MetamericEditorInfo info) {
    met_trace();
    
    fmt::print(
      "Starting {}\n  range   : {}-{} nm\n  samples : {}\n  bases   : {}\n  loading : {}\n",
      info.app_title,
      wavelength_min, wavelength_max, wavelength_samples, wavelength_bases, 
      info.scene_path.string());

    // Scheduler is responsible for handling application tasks, resources, and runtime loop
    LinearScheduler scheduler;

    // Initialize window (OpenGL context), as a resource owned by the scheduler
    auto &window = scheduler.global("window").init<gl::Window>({ 
      .size  = info.app_size, 
      .title = info.app_title, 
      .flags = gl::WindowFlags::eVisible   | gl::WindowFlags::eFocused 
             | gl::WindowFlags::eDecorated | gl::WindowFlags::eResizable 
             | gl::WindowFlags::eMSAA met_debug_insert(| gl::WindowFlags::eDebug)
    }).getw<gl::Window>();

    // Initialize OpenGL debug messages, if requested
    if constexpr (met_enable_debug) {
      gl::debug::enable_messages(gl::DebugMessageSeverity::eLow, gl::DebugMessageTypeFlags::eAll);
      gl::debug::insert_message("OpenGL debug messages are active!", gl::DebugMessageSeverity::eLow);
    }

    // Initialize program cache, as a resource owned by the scheduler
    scheduler.global("cache").set<gl::ProgramCache>({ });

    // Initialize scene handler, as a resource owned by the scheduler
    auto &scene = scheduler.global("scene").set<Scene>({
      /* ... */
    }).getw<Scene>();

    // Load scene if a scene path is provided
    if (!info.scene_path.empty())
      scene.load(info.scene_path);

    // Create and start runtime loop
    submit_metameric_editor_schedule(scheduler);
    while (!window.should_close())
      scheduler.run();
  }
} // namespace met

// Application entry point
int main() {
  /* try { */
    met::create_metameric_editor({ /* .scene_path = "C:/Users/markv/Desktop/Meshes/CoffeeCart_01_4k.blend/CoffeeCart_01_4k.json" */ });
  /* } catch (const std::exception &e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  } */
  return EXIT_SUCCESS;
}