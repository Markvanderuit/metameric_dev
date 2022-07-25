#include <metameric/core/state.hpp>
#include <metameric/components/views/mapping_viewer.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  MappingViewer::MappingViewer(const std::string &name)
  : detail::AbstractTask(name) { }

  void MappingViewer::init(detail::TaskInitInfo &info) {
    // TODO delete if unnecessary
  }

  void MappingViewer::eval(detail::TaskEvalInfo &info) {
    auto &e_app_data = info.get_resource<ApplicationData>("global", "application_data");
    auto &e_prj_data = e_app_data.project_data;

    if (ImGui::Begin("Spectral mappings")) {
      // ... Make list here
    }
    ImGui::End();
  }
} // namespace met 