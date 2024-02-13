#include <metameric/components/views/mmv_viewport/task_draw_mmv.hpp>

namespace met {
  bool DrawMMVTask::is_active(SchedulerHandle &info) {
    met_trace();
    return true;
  }

  void DrawMMVTask::init(SchedulerHandle &info) {
    met_trace();
    
  }

  void DrawMMVTask::eval(SchedulerHandle &info) {
    met_trace();
    
  }
} // namespace met