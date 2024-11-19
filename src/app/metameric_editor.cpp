#include <metameric/scene/scene.hpp>
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
    // Direct load scene path; optionally allowed to fail for a default scene load
    fs::path scene_path      = "";
    bool     scene_fail_safe = false;

    // Shader cache path
    fs::path shader_path = "shaders/shaders.bin";

    // Window settings
    eig::Array2u app_size  = { 1800, 1024 };
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

    // Enable OpenGL debug messages, if requested
    if constexpr (met_enable_debug) {
      gl::debug::enable_messages(gl::DebugMessageSeverity::eLow, gl::DebugMessageTypeFlags::eAll);
      gl::debug::insert_message("OpenGL messages enabled", gl::DebugMessageSeverity::eLow);
    }

    // Initialize program cache as resource owned by the scheduler;
    // load from file if a path is specified
    scheduler.global("cache").set<gl::detail::ProgramCache>({ });
    if (!info.shader_path.empty() && fs::exists(info.shader_path))
      scheduler.global("cache").getw<gl::detail::ProgramCache>().load(info.shader_path);

    // Initialize scene data as resources owned by the scheduler
    // load from file if a path is specified
    scheduler.global("scene").set<Scene>(scheduler.global("cache"));
    if (!info.scene_path.empty() && (info.scene_fail_safe || fs::exists(info.scene_path)))
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
  try {
    // Supply a default scene; this can fail silently
    met::metameric_editor({ .scene_path = "data/cornell_box.json", .scene_fail_safe = true });
  } catch (const std::exception &e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}