#pragma once

#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_task.hpp>

namespace met {
  namespace detail {

  } // namespace detail

  class ViewportInputFaceTask : public detail::AbstractTask {
  public:
    ViewportInputFaceTask(const std::string &name)
    : detail::AbstractTask(name, true) { }

    void init(detail::TaskInitInfo &info) override {
      met_trace_full();

      // ...
    }

    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();

      // ...
    }
  };
}