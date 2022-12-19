#include <metameric/core/spectrum.hpp>
#include <metameric/core/data.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/task_spectra_editor.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  SpectraEditorTask::SpectraEditorTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void SpectraEditorTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();
    
    if (ImGui::Begin("Spectra editor")) {
      // Get external shared resources
      auto &e_appl_data = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_proj_data = e_appl_data.project_data;

      if (ImGui::BeginListBox("Illuminant spectra")) {
        auto &e_illuminants = e_proj_data.illuminants;
        for (uint i = 0; i < e_illuminants.size(); ++i) {
          auto &[key, illuminant] = e_illuminants[i];
          if (ImGui::Selectable(key.c_str(), false)) {
            // ...
          }
        }
        ImGui::EndListBox();
      }

      if (ImGui::BeginListBox("Color matching functions")) {
        auto &e_cmfs = e_proj_data.cmfs;
        for (uint i = 0; i < e_cmfs.size(); ++i) {
          auto &[key, cmfs] = e_cmfs[i];
          if (ImGui::Selectable(key.c_str(), false)) {
            // ...
          }
        }
        ImGui::EndListBox();
      }
      
    }
    ImGui::End();
  }
} // namespace met