#pragma once

#include <metameric/core/data.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_task.hpp>

namespace met {
  namespace detail {

  } // namespace detail

  class ViewportInputEdgeTask : public detail::AbstractTask {
  public:
    ViewportInputEdgeTask(const std::string &name)
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