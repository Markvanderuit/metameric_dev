#include <metameric/core/io.hpp>
#include <metameric/core/data.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/tree.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/schedule.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/window.hpp>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <cstdlib>
#include <exception>

namespace met {
  struct ApplicationCreateInfo {
    // In case of a existing project load
    fs::path project_path = "";

    // Application color theme
    ApplicationData::ColorMode color_mode = ApplicationData::ColorMode::eDark;
  };

  namespace detail {
    void init_state(LinearScheduler &scheduler, ApplicationCreateInfo info) {
      met_trace();
      
      ApplicationData data = { .color_mode = info.color_mode };
      if (!info.project_path.empty()) {
        data.load(info.project_path);
      } else {
        data.unload();
      }
      
      // TODO: remove
      auto loaded_tree = io::load_json("resources/misc/tree.json").get<BasisTreeNode>();
      data.loaded_basis = loaded_tree.basis;
      data.loaded_basis_mean = loaded_tree.basis_mean;

      scheduler.global("appl_data").set(std::move(data));
    }

    void init_schedule(LinearScheduler &scheduler) {
      auto &app_data = scheduler.global("appl_data").writeable<ApplicationData>();
      if (app_data.project_save == ProjectSaveState::eSaved) {
        submit_schedule_main(scheduler);
      } else {
        submit_schedule_empty(scheduler);
      }
    }
  } // namespace detail                 

  void create_application(ApplicationCreateInfo info) {
    fmt::print("Metameric format\n  min : {} nm\n  max : {} nm\n  samples: {}\n",
      wavelength_min, wavelength_max, wavelength_samples);

    // Scheduler is responsible for handling application tasks and resources
    LinearScheduler scheduler;

    // Initialize OpenGL context (and primary window) and submit to scheduler
    auto &window = scheduler.global("window").init<gl::Window>({ 
      .size  = { 1680, 1024 }, 
      .title = "Metameric",
      .flags = gl::WindowCreateFlags::eVisible
             | gl::WindowCreateFlags::eFocused   
             | gl::WindowCreateFlags::eDecorated
             | gl::WindowCreateFlags::eResizable
             | gl::WindowCreateFlags::eMSAA               // Enable support for MSAA framebuffers
#if defined(NDEBUG) || defined(MET_ENABLE_EXCEPTIONS)
             | gl::WindowCreateFlags::eDebug              // Enable support for OpenGL debug output
#endif
    }).writeable<gl::Window>();

    // Enable OpenGL debug messages, ignoring notification-type messages
#if defined(NDEBUG) || defined(MET_ENABLE_EXCEPTIONS)
    gl::debug::enable_messages(gl::DebugMessageSeverity::eLow, gl::DebugMessageTypeFlags::eAll);
    gl::debug::insert_message("OpenGL debug messages are active!", gl::DebugMessageSeverity::eLow);
#endif

    ImGui::Init(window, info.color_mode == ApplicationData::ColorMode::eDark);
    
    // Initialize major application components on startup
    detail::init_state(scheduler, info);
    detail::init_schedule(scheduler);

    // Main runtime loop
    while (!window.should_close())
      scheduler.run();
    
    // Tear down remaining components
    ImGui::Destr();
  }
} // namespace met

int main() {
  /* try { */
    met::create_application({ .color_mode    = met::ApplicationData::ColorMode::eDark });
  /* } catch (const std::exception &e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  } */
  return EXIT_SUCCESS;
}