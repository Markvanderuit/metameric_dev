#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <functional>

namespace met {
  class LambdaTask : public detail::AbstractTask {
    using InitType = std::function<void(detail::TaskInfo &)>;
    using EvalType = std::function<void(detail::TaskInfo &)>;
    using DstrType = std::function<void(detail::TaskInfo &)>;

    InitType _init;
    EvalType _eval;
    DstrType _dstr;

  public:
    LambdaTask(const std::string &name, EvalType eval)
    : detail::AbstractTask(name),
      _eval(eval) { }

    LambdaTask(const std::string &name, InitType init, EvalType eval)
    : detail::AbstractTask(name),
      _init(init), _eval(eval) { }

    LambdaTask(const std::string &name, InitType init, EvalType eval, DstrType dstr)
    : detail::AbstractTask(name),
      _init(init), _eval(eval), _dstr(dstr) { }

    void init(detail::TaskInfo &init_info) override {
      met_trace_full();

      if (_init)
        _init(init_info);
    }

    void eval(detail::TaskInfo &eval_info) override {
      met_trace_full();

      _eval(eval_info);
    }

    void dstr(detail::TaskInfo &dstr_info) override {
      met_trace_full();

      if (_dstr)
        _dstr(dstr_info);
    }
  };
} // namespace met