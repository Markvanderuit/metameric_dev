#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  class MappingViewer : public detail::AbstractTask {
  public:
    MappingViewer(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met