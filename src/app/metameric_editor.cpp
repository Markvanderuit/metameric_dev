#include <metameric/core/scene.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/schedule.hpp>
#include <small_gl/window.hpp>
#include <small_gl/program.hpp>
#include <small_gl/detail/program_cache.hpp>
#include <cstdlib>
#include <exception>

namespace met {
  constexpr auto window_flags 
    = gl::WindowFlags::eVisible   | gl::WindowFlags::eFocused 
    | gl::WindowFlags::eDecorated | gl::WindowFlags::eResizable 
    | gl::WindowFlags::eMSAA met_debug_insert(| gl::WindowFlags::eDebug);

  // Application create settings
  struct MetamericEditorInfo {
    // Direct load scene path
    fs::path scene_path = "";

    // Shader cache path
    fs::path shader_path = "resources/shaders/shaders.bin";

    // Window settings
    eig::Array2u app_size  = { 2560, 1080 };
    std::string  app_title = "Metameric Editor";
  };

  // Application create function
  void metameric_editor(MetamericEditorInfo info) {
    met_trace();
    
    fmt::print(
      "Starting {}\n  range   : {}-{} nm\n  samples : {}\n  bases   : {}\n  loading : {}\n",
      info.app_title,
      wavelength_min, wavelength_max, wavelength_samples, wavelength_bases, 
      info.scene_path.string());

    // Scheduler is responsible for handling application tasks, 
    // task resources, and the program runtime loop
    LinearScheduler scheduler;

    // Initialize window (OpenGL context), as a resource owned by the scheduler
    auto &window = scheduler.global("window").init<gl::Window>({ 
      .size  = info.app_size, 
      .title = info.app_title, 
      .flags = window_flags
    }).getw<gl::Window>();
    // window.set_swap_interval(0);

    // Enable OpenGL debug messages, if requested
    if constexpr (met_enable_debug) {
      gl::debug::enable_messages(gl::DebugMessageSeverity::eLow, gl::DebugMessageTypeFlags::eAll);
      gl::debug::insert_message("OpenGL debug messages are active!", gl::DebugMessageSeverity::eLow);
    }

    // Initialize program cache as resource ownedd by the scheduler;
    // load from file if a path is specified
    if (!info.shader_path.empty() && fs::exists(info.shader_path)) {
      scheduler.global("cache").set<gl::detail::ProgramCache>(info.shader_path);
    } else {
      scheduler.global("cache").set<gl::detail::ProgramCache>({ });
    }

    // Initialize program cache and scene data as resources owned by the scheduler
    // and not a specific schedule task
    scheduler.global("scene").set<Scene>({ });

    // Load scene if a scene path is provided
    if (!info.scene_path.empty())
      scheduler.global("scene").getw<Scene>().load(info.scene_path);

    // Load appropriate set of schedule tasks, then start the runtime loop
    submit_metameric_editor_schedule(scheduler);
    while (!window.should_close())
      scheduler.run();

    // Attempt to save shader cache, if exists
    if (!info.shader_path.empty())
      scheduler.global("cache").getr<gl::detail::ProgramCache>().save(info.shader_path);
  }
} // namespace met

// Application entry point
int main() {
  /* try { */
    met::metameric_editor({ /* .scene_path = "path/to/file.json" */ });
  /* } catch (const std::exception &e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  } */
  return EXIT_SUCCESS;
}