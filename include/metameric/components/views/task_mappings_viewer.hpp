#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  class MappingsViewerTask : public detail::AbstractTask {
    void handle_tooltip(detail::TaskEvalInfo &info, uint texture_i) const;
    void handle_popout(detail::TaskEvalInfo &info, uint texture_i) const;

  public:
    MappingsViewerTask(const std::string &name);
    
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met
