#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/texture.hpp>

namespace met {
  class ImageViewerTask : public detail::AbstractTask {
    gl::Texture2d3f m_texture;
    
  public:
    ImageViewerTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met