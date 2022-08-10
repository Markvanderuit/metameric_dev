#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/utility.hpp>
#include <functional>

namespace met {
  class LambdaTask : public detail::AbstractTask {
    using InitType = std::function<void(detail::TaskInitInfo &)>;
    using EvalType = std::function<void(detail::TaskEvalInfo &)>;
    using DstrType = std::function<void(detail::TaskDstrInfo &)>;

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

    void init(detail::TaskInitInfo &init_info) override {
      met_declare_trace_zone();

      if (_init)
        _init(init_info);
    }

    void eval(detail::TaskEvalInfo &eval_info) override {
      met_declare_trace_zone();

      _eval(eval_info);
    }

    void dstr(detail::TaskDstrInfo &dstr_info) override {
      met_declare_trace_zone();

      if (_dstr)
        _dstr(dstr_info);
    }
  };
} // namespace met