#include <metameric/core/detail/glm.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/image_viewer.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <fmt/core.h>

namespace met {
  ImageViewerTask::ImageViewerTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void ImageViewerTask::init(detail::TaskInitInfo &info) {
    // Get externally shared resources
    auto &e_texture_obj = info.get_resource<io::TextureData<Color>>("global", "color_texture_buffer_cpu");

    // Load texture data into gl texture
    m_texture = gl::Texture2d3f({ .size = e_texture_obj.size, .data = as_typed_span<float>(e_texture_obj.data) });
  }
  
  void ImageViewerTask::eval(detail::TaskEvalInfo &info) {
    if (ImGui::Begin("Input")) {
      auto viewport_size = static_cast<glm::vec2>(ImGui::GetWindowContentRegionMax().x)
                         - static_cast<glm::vec2>(ImGui::GetWindowContentRegionMin().x);
      auto texture_aspect = static_cast<float>(m_texture.size().y) 
                          / static_cast<float>(m_texture.size().x);
      ImGui::Image(ImGui::to_ptr(m_texture.object()), viewport_size * glm::vec2(1, texture_aspect));
    }
    ImGui::End();

    if (ImGui::Begin("Mapped")) {
      // Get external resources
      auto &e_color_texture = info.get_resource<gl::Texture2d4f>("mapping", "color_texture");
      // auto &e_color_texture = info.get_resource<gl::Texture2d4f>("mapping", "color_texture");

      auto viewport_size = static_cast<glm::vec2>(ImGui::GetWindowContentRegionMax().x)
                         - static_cast<glm::vec2>(ImGui::GetWindowContentRegionMin().x);
      auto texture_aspect = static_cast<float>(e_color_texture.size().y) 
                          / static_cast<float>(e_color_texture.size().x);
                          
      ImGui::Image(ImGui::to_ptr(e_color_texture.object()), viewport_size * glm::vec2(1, texture_aspect));
    }
    ImGui::End();
  }
} // namespace met