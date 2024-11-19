#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <functional>

namespace met::detail {
  class LambdaTask : public detail::TaskNode {
    using init_type = std::function<void(SchedulerHandle &)>;
    using eval_type = std::function<void(SchedulerHandle &)>;
    using dstr_type = std::function<void(SchedulerHandle &)>;

    init_type m_init;
    eval_type m_eval;
    dstr_type m_dstr;

  public:
    LambdaTask(eval_type eval)
    : m_eval(eval) { }

    LambdaTask(init_type init, eval_type eval)
    : m_init(init), m_eval(eval) { }

    LambdaTask(init_type init, eval_type eval, dstr_type dstr)
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
} // namespace met::detail