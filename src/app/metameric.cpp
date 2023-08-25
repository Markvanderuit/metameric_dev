#include <metameric/core/io.hpp>
#include <metameric/core/data.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/tree.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/schedule.hpp>
#include <small_gl/window.hpp>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <cstdlib>
#include <exception>

namespace met {
  // Application settings object with sensible defaults
  class ApplicationCreateInfo {
    using ColorMode = ApplicationData::ColorMode;

  public:
    // Project settings
    fs::path project_path = "";

    // Window settings
    eig::Array2u app_size  = { 1680, 1024 };
    std::string  app_title = "Metameric";
    ColorMode    app_cmode = ColorMode::eDark;

    // Misc settings
    fs::path basis_path = "resources/misc/tree.json";
  };

  void create_application(ApplicationCreateInfo info) {
    met_trace();
    
    fmt::print(
      "Starting Metameric\n  range   : {}-{} nm\n  samples : {}\n  bases   : {}\n  loading : {}\n",
      wavelength_min, wavelength_max, wavelength_samples, wavelength_bases, info.project_path.string());

    // Scheduler is responsible for handling application tasks, resources, and runtime loop
    LinearScheduler scheduler;

    // Initialize window (OpenGL context), as a resource owned by the scheduler
    auto &window = scheduler.global("window").init<gl::Window>({ 
      .size  = info.app_size, 
      .title = info.app_title, 
      .flags = gl::WindowCreateFlags::eVisible   | gl::WindowCreateFlags::eFocused 
             | gl::WindowCreateFlags::eDecorated | gl::WindowCreateFlags::eResizable 
             | gl::WindowCreateFlags::eMSAA met_debug_insert(| gl::WindowCreateFlags::eDebug)
    }).writeable<gl::Window>();

    // Initialize OpenGL debug messages, if requested
    if constexpr (met_enable_debug) {
      gl::debug::enable_messages(gl::DebugMessageSeverity::eLow, gl::DebugMessageTypeFlags::eAll);
      gl::debug::insert_message("OpenGL debug messages are active!", gl::DebugMessageSeverity::eLow);
    }

    // Initialize application data component, as a resource owned by the scheduler
    auto loaded_tree = io::load_json(info.basis_path).get<BasisTreeNode>();
    auto &appl_data = scheduler.global("appl_data").set<ApplicationData>({
      .loaded_basis      = loaded_tree.basis,
      .loaded_basis_mean = loaded_tree.basis_mean,
      .color_mode        = info.app_cmode
    }).writeable<ApplicationData>();

    // Load project data if a path is provided
    if (!info.project_path.empty())
      appl_data.load(info.project_path);

    // Create and start runtime loop
    submit_schedule(scheduler);
    while (!window.should_close())
      scheduler.run();
  }
} // namespace met

int main() {
 /*  try { */
    met::create_application({
      // .project_path = "C:/Users/mark/Desktop/cart/CoffeeCart_01_4k.json"
    });
  /* } catch (const std::exception &e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  } */
  return EXIT_SUCCESS;
}