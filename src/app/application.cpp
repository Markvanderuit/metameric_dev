#include <metameric/core/io.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/knn.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/data.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/schedule.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/app/application.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <small_gl/window.hpp>
#include <small_gl_parser/parser.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <execution>
#include <ranges>

namespace met {
  namespace detail {
    void init_state(LinearScheduler &scheduler, ApplicationCreateInfo info) {
      met_trace();
      
      ApplicationData data = { .color_mode = info.color_mode };
      if (!info.project_path.empty()) {
        data.load(info.project_path);
      } else {
        data.unload();
      }
      
      // data.loaded_basis = io::load_basis("resources/misc/basis.txt");
      // data.loaded_basis_avg = io::load_spec("resources/misc/basis_avg.spd");
      data.loaded_tree_root = io::load_json("resources/misc/tree.json").get<BasisTreeNode>();

      // TODO: remove
      // data.loaded_tree_root.basis = data.loaded_basis;
      // data.loaded_tree_root.basis_mean = data.loaded_basis_avg;
      data.loaded_basis = data.loaded_tree_root.basis;
      data.loaded_basis_avg = data.loaded_tree_root.basis_mean;

      scheduler.insert_resource("app_data", std::move(data));
    }

    void init_parser(LinearScheduler &scheduler) {
      glp::Parser parser;
      parser.add_string("MET_SUBGROUP_SIZE", 
        std::to_string(gl::state::get_variable_int(gl::VariableName::eSubgroupSize)));
      scheduler.insert_resource("glsl_parser", std::move(parser));
    }

    void init_schedule(LinearScheduler &scheduler) {
      auto &app_data = scheduler.get_resource<ApplicationData>(global_key, "app_data");
      if (app_data.project_save == SaveFlag::eSaved) {
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
    gl::Window &window = scheduler.emplace_resource<gl::Window>("window", { 
      .size  = { 1680, 1024 }, 
      .title = "Metameric",
      .flags = gl::WindowCreateFlags::eVisible
             | gl::WindowCreateFlags::eFocused   
             | gl::WindowCreateFlags::eDecorated
             | gl::WindowCreateFlags::eResizable
             | gl::WindowCreateFlags::eSRGB               // Support sRGB-corrected framebuffers
             | gl::WindowCreateFlags::eMSAA               // Support MSAA framebuffers
#if defined(NDEBUG) || defined(MET_ENABLE_DBG_EXCEPTIONS)
             | gl::WindowCreateFlags::eDebug              // Support OpenGL debug output
#endif
    });

    // Enable OpenGL debug messages, ignoring notification-type messages
#if defined(NDEBUG) || defined(MET_ENABLE_DBG_EXCEPTIONS)
    gl::debug::enable_messages(gl::DebugMessageSeverity::eLow, gl::DebugMessageTypeFlags::eAll);
    gl::debug::insert_message("OpenGL debug messages are active!", gl::DebugMessageSeverity::eLow);
#endif

    ImGui::Init(window, info.color_mode == AppColorMode::eDark);
    
    // Initialize major application components on startup
    detail::init_parser(scheduler);
    detail::init_state(scheduler, info);
    detail::init_schedule(scheduler);

    // Main runtime loop
    while (!window.should_close()) { 
      scheduler.run();
    } 
    
    ImGui::Destr();
  }
} // namespace met