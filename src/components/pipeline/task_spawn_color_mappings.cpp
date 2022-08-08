#include <metameric/components/tasks/task_spawn_color_mappings.hpp>
#include <metameric/components/tasks/task_gen_color_mapping.hpp>
#include <metameric/core/state.hpp>

namespace met {
  static const std::string gen_subtask_key = "gen_color_mapping_";
  using SubtaskType = GenColorMappingTask;

  SpawnColorMappingsTask::SpawnColorMappingsTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void SpawnColorMappingsTask::init(detail::TaskInitInfo &info) {
    // Get shared resources
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    
    // Set initial sensible value
    m_tasks_n = e_app_data.loaded_mappings.size();

    // Spawn initial subtasks exactly after this task
    std::string prev_name = name();
    for (uint i = 0; i < m_tasks_n; ++i) {
      std::string curr_name = fmt::format("{}{}", gen_subtask_key, i);
      info.emplace_task_after<SubtaskType>(prev_name, curr_name, i);
      prev_name = curr_name;
    }
  }

  void SpawnColorMappingsTask::dstr(detail::TaskDstrInfo &info) {
    // Remove existing subtasks
    for (uint i = 0; i < m_tasks_n; ++i) {
      info.remove_task(fmt::format("{}{}", gen_subtask_key, i));
    }
  }

  void SpawnColorMappingsTask::eval(detail::TaskEvalInfo &info) {
    // Get externally shared resources
    auto &e_app_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    uint e_mappings_n = e_app_data.loaded_mappings.size();

    // Spawn tasks to suit an increased right nr. of mappings
    for (; m_tasks_n < e_mappings_n; ++m_tasks_n) {
      info.emplace_task_after<SubtaskType>(fmt::format("{}{}", gen_subtask_key, m_tasks_n - 1), 
                                           fmt::format("{}{}", gen_subtask_key, m_tasks_n), 
                                           m_tasks_n);
    }

    // Remove tasks to suit a decreased nr. of mappings
    for (; m_tasks_n > e_mappings_n; --m_tasks_n) {
      info.remove_task(fmt::format("{}{}", gen_subtask_key, m_tasks_n - 1));
    }
  }
} // namespace met