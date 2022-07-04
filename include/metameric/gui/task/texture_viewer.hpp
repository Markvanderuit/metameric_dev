#pragma once

#include <metameric/gui/detail/linear_scheduler/task.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/program.hpp>
#include <vector>

namespace met {
  class TextureViewerTask : public detail::AbstractTask {
    gl::Program                  m_program_uplifting;
    gl::Texture2d4f              m_texture_base;
    std::vector<gl::Texture2d3f> m_texture_uplifting;
    std::vector<gl::Buffer>      m_illuminants;

  public:
    TextureViewerTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met