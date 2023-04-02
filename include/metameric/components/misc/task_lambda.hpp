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

    InitType m_init;
    EvalType m_eval;
    DstrType m_dstr;

  public:
    LambdaTask(EvalType eval)
    : m_eval(eval) { }

    LambdaTask(InitType init, EvalType eval)
    : m_init(init), m_eval(eval) { }

    LambdaTask(InitType init, EvalType eval, DstrType dstr)
    : m_init(init), m_eval(eval), m_dstr(dstr) { }

    void init(SchedulerHandle &init_info) override {
      met_trace();

      if (m_init)
        m_init(init_info);
    }

    void eval(SchedulerHandle &eval_info) override {
      met_trace();

      m_eval(eval_info);
    }

    void dstr(SchedulerHandle &dstr_info) override {
      met_trace();

      if (m_dstr)
        m_dstr(dstr_info);
    }
  };
} // namespace met