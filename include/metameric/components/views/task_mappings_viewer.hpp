#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  class MappingsViewerTask : public detail::AbstractTask {

  public:
    MappingsViewerTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met
