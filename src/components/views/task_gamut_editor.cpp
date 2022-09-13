#include <metameric/components/views/task_gamut_editor.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>

namespace met {
  GamutEditorTask::GamutEditorTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GamutEditorTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

  }

  void GamutEditorTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_app_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_gamut_idx = info.get_resource<int>("viewport", "gamut_selection");
    auto &e_gamut     = e_app_data.project_data.rgb_gamut;
    
    if (e_gamut_idx >= 0 && ImGui::Begin("Gamut editor")) {
      // Can't modify the gamut, should modify offsets
      // Colr &c = e_gamut[e_gamut_idx];
      // ImGui::SliderFloat3("Offset", c.data(), -1.f, 1.f);
      fmt::print("{}\n", e_gamut_idx);
      ImGui::Text("Hi!");
      ImGui::End();
    }
  }
} // namespace met