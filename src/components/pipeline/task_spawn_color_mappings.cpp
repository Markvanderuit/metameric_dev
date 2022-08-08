#include <metameric/components/tasks/task_spawn_color_mappings.hpp>
#include <metameric/components/tasks/task_gen_color_mapping.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/state.hpp>

namespace met {
  constexpr auto gen_subtask_tex_fmt = FMT_COMPILE("gen_color_mapping_{}");
  
  using SubtaskType = GenColorMappingTask;

  SpawnColorMappingsTask::SpawnColorMappingsTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void SpawnColorMappingsTask::init(detail::TaskInitInfo &info) {
    // Get shared resources
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    
    // Set initial sensible value
    uint i_tasks_n = e_app_data.loaded_mappings.size();

    // Spawn initial subtasks exactly after this task
    std::string prev_name = name();
    for (uint i = 0; i < i_tasks_n; ++i) {
      std::string curr_name = fmt::format(gen_subtask_tex_fmt, i);
      info.emplace_task_after<SubtaskType>(prev_name, curr_name, i);
      prev_name = curr_name;
    }

    // Share current nr. of spawned tasks
    info.insert_resource("tasks_n", std::move(i_tasks_n));
  }

  void SpawnColorMappingsTask::dstr(detail::TaskDstrInfo &info) {
    // Get shared resources
    auto &i_tasks_n  = info.get_resource<uint>("tasks_n");

    // Remove existing subtasks
    for (uint i = 0; i < i_tasks_n; ++i) {
      info.remove_task(fmt::format(gen_subtask_tex_fmt, i));
    }
  }

  void SpawnColorMappingsTask::eval(detail::TaskEvalInfo &info) {
    // Get shared resources
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &i_tasks_n  = info.get_resource<uint>("tasks_n");
    uint e_mappings_n = e_app_data.loaded_mappings.size();

    // Spawn tasks to suit an increased right nr. of mappings
    for (; i_tasks_n < e_mappings_n; ++i_tasks_n) {
      info.emplace_task_after<SubtaskType>(fmt::format(gen_subtask_tex_fmt, i_tasks_n - 1), 
                                           fmt::format(gen_subtask_tex_fmt, i_tasks_n), 
                                           i_tasks_n);
    }

    // Remove tasks to suit a decreased nr. of mappings
    for (; i_tasks_n > e_mappings_n; --i_tasks_n) {
      info.remove_task(fmt::format(gen_subtask_tex_fmt, i_tasks_n - 1));
    }
  }
} // namespace met