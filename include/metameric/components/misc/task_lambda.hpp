#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <functional>

namespace met {
  class LambdaTask : public detail::TaskNode {
    using InitType = std::function<void(SchedulerHandle &)>;
    using EvalType = std::function<void(SchedulerHandle &)>;
    using DstrType = std::function<void(SchedulerHandle &)>;

    InitType _init;
    EvalType _eval;
    DstrType _dstr;

  public:
    LambdaTask(EvalType eval)
    : _eval(eval) { }

    LambdaTask(InitType init, EvalType eval)
    : _init(init), _eval(eval) { }

    LambdaTask(InitType init, EvalType eval, DstrType dstr)
    : _init(init), _eval(eval), _dstr(dstr) { }

    void init(SchedulerHandle &init_info) override {
      met_trace_full();

      if (_init)
        _init(init_info);
    }

    void eval(SchedulerHandle &eval_info) override {
      met_trace_full();

      _eval(eval_info);
    }

    void dstr(SchedulerHandle &dstr_info) override {
      met_trace_full();

      if (_dstr)
        _dstr(dstr_info);
    }
  };
} // namespace met