#include <metameric/components/views/mmv_viewport/task_gen_mmv.hpp>

namespace met {
  bool GenMMVTask::is_active(SchedulerHandle &info) {
    met_trace();
    return true;
  }

  void GenMMVTask::init(SchedulerHandle &info) {
    met_trace();
    
  }

  void GenMMVTask::eval(SchedulerHandle &info) {
    met_trace();
    
  }
} // namespace met