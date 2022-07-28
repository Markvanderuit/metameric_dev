#pragma once

#include <functional>
#include <metameric/core/scheduler.hpp>

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
      if (_init)
        _init(init_info);
    }

    void eval(detail::TaskEvalInfo &eval_info) override {
      _eval(eval_info);
    }

    void dstr(detail::TaskDstrInfo &dstr_info) override {
      if (_dstr)
        _dstr(dstr_info);
    }
  };
} // namespace met