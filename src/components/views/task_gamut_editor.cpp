#include <metameric/components/views/task_gamut_editor.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <small_gl/texture.hpp>

namespace met {
  GamutEditorTask::GamutEditorTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GamutEditorTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Share resources
    info.insert_resource<gl::Texture2d3f>("draw_texture", gl::Texture2d3f());
  }

  void GamutEditorTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();
    
    // Get shared resources
    auto &e_app_data     = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_gamut_idx    = info.get_resource<int>("viewport", "gamut_selection");
    auto &e_gamut        = e_app_data.project_data.rgb_gamut;
    auto &i_draw_texture = info.get_resource<gl::Texture2d3f>("draw_texture");
    
    if (ImGui::Begin("Gamut editor")) {
      // Compute viewport size minus ImGui's tab bars etc
      // (Re-)create viewport texture if necessary; attached framebuffers are resized separately
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f texture_size = { viewport_size.x(), viewport_size.x() };
      
      if (!i_draw_texture.is_init() || (i_draw_texture.size() != viewport_size.cast<uint>()).all()) {
        i_draw_texture = {{ .size = viewport_size.cast<uint>() }};
      }

      // Insert image, applying viewport texture to viewport; texture can be safely drawn 
      // to later in the render loop. Flip y-axis UVs to obtain the correct orientation.
      ImGui::Image(ImGui::to_ptr(i_draw_texture.object()), viewport_size, eig::Vector2f(0, 1), eig::Vector2f(1, 0));

      // Handle input
      if (ImGui::IsItemHovered()) {
        // ...
      }

      ImGui::End();
    }
  }
} // namespace met