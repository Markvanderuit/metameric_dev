#include <metameric/core/state.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/image_viewer.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  ImageViewerTask::ImageViewerTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void ImageViewerTask::init(detail::TaskInitInfo &info) {
    // Get externally shared resources
    auto &e_rgb_texture = info.get_resource<ApplicationData>(global_key, "app_data").rgb_texture;

    // Load texture data into gl texture
    m_texture = gl::Texture2d3f({ .size = e_rgb_texture.size(), .data = cast_span<float>(e_rgb_texture.data()) });
  }
  
  void ImageViewerTask::eval(detail::TaskEvalInfo &info) {
    if (ImGui::Begin("Input")) {
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax().x)
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin().x);
      auto texture_aspect = static_cast<float>(m_texture.size().y()) 
                          / static_cast<float>(m_texture.size().x());
      ImGui::Image(ImGui::to_ptr(m_texture.object()), (viewport_size * eig::Array2f(1, texture_aspect)).eval());
    }
    ImGui::End();

    if (ImGui::Begin("Mapped")) {
      // Get external resources
      auto &e_color_texture = info.get_resource<gl::Texture2d4f>("comp_color_mapping", "color_texture");

      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax().x)
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin().x);
      auto texture_aspect = static_cast<float>(m_texture.size().y()) 
                          / static_cast<float>(m_texture.size().x());
                          
      ImGui::Image(ImGui::to_ptr(e_color_texture.object()), (viewport_size * eig::Array2f(1, texture_aspect)).eval());
    }
    ImGui::End();
  }
} // namespace met