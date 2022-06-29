#include <metameric/core/detail/glm.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/gui/task/texture_viewer.hpp>
#include <metameric/gui/detail/imgui.hpp>

namespace met {
  TextureViewerTask::TextureViewerTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void TextureViewerTask::init(detail::TaskInitInfo &info) {
    // Get externally shared resources
    auto &e_texture_obj = info.get_resource<io::TextureData<float>>("global", "texture_data");

    // Load texture data into gl texture
    m_texture_base = gl::Texture2d3f({ .size = e_texture_obj.size, .data = e_texture_obj.data });
  }

  void TextureViewerTask::eval(detail::TaskEvalInfo &info) {
    for (auto &texture : m_texture_uplifting) {

    }
  }
} // namespace met