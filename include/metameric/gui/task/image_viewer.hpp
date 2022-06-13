#pragma once

#include <small_gl/texture.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>

namespace met {
  class ImageViewerTask : public detail::AbstractTask {
    gl::Texture2d3f m_texture;
    
  public:
    ImageViewerTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met