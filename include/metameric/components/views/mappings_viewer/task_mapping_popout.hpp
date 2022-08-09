#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  class MappingsPopoutTask : public detail::AbstractTask {
  public:
    MappingsPopoutTask(const std::string &name)
    : detail::AbstractTask(name) { }
    
    void init(detail::TaskInitInfo &) override {

    }
    
    void eval(detail::TaskEvalInfo &) override {

    }
  };
} // namespace met