#pragma once

#include <imgui.h>
#include <small_gl/texture.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>

namespace met {
  class ViewportTask : public detail::AbstractTask {
    using Tex3f2d = gl::Texture<float, 2, 3>;
    using Tex3f2dInfo = gl::TextureCreateInfo<float, 2>;

    Tex3f2d     _frame_texture;
    gl::Array3f _clear_color = { 255.f, 0.f, 255.f };

  private:
    template <typename T>
    gl::Array2i to_array(T t) { return { static_cast<int>(t[0]), static_cast<int>(t[1]) }; }

    template <typename T>
    ImVec2 to_imvec2(T t) { return { static_cast<float>(t[0]), static_cast<float>(t[1]) }; }

    template <typename T, typename Array>
    std::span<T> to_span(const Array &v) { return std::span<T>((T *) v.data(), v.rows() * v.cols()); }

    void * to_imgui_ptr(GLuint obj) { return (void *) (size_t) obj; }

  public:
    ViewportTask(const std::string &name)
    : detail::AbstractTask(name) { }

    void init(detail::TaskInitInfo &info) override {
      // _frame_texture = Tex3f2d({ .size = { 128, 128} });

      /* info.emplace_resource<Tex3f2d, Tex3f2dInfo>("frame_texture", {
        .size = { 128, 128 }
      }); */
    }

    void eval(detail::TaskEvalInfo &info) override {
      // Begin window draw
      auto flags = ImGuiWindowFlags_NoDecoration;
      ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f });
      ImGui::Begin("Viewport", 0, flags);

      // Resize or initialize viewport draw texture
      const auto viewport_size = to_array(ImGui::GetWindowContentRegionMax())
                               - to_array(ImGui::GetWindowContentRegionMin());
      if (!_frame_texture.is_init() || (_frame_texture.size() != viewport_size).any()) {
        _frame_texture = Tex3f2d({ .size = viewport_size });
        _frame_texture.clear(to_span<float>(_clear_color));
      }

      // Add texture to viewport
      ImGui::Image(to_imgui_ptr(_frame_texture.object()), to_imvec2(_frame_texture.size()));

      // End window draw
      ImGui::End();
      ImGui::PopStyleVar();
      ImGui::PopStyleVar();
      ImGui::PopStyleVar();
    }
  };
} // namespace met