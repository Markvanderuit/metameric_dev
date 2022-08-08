#include <metameric/core/math.hpp>
#include <metameric/core/state.hpp>
#include <metameric/components/views/task_mappings_viewer.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  MappingsViewerTask::MappingsViewerTask(const std::string &name)
  : detail::AbstractTask(name) { }


  void MappingsViewerTask::init(detail::TaskInitInfo &info) {

  }
  
  void MappingsViewerTask::eval(detail::TaskEvalInfo &info) {
    if (ImGui::Begin("Mappings viewer")) {
      // Get externally shared resources
      auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");

      
    }
  }
} // namespace met