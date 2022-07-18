#include <metameric/core/scheduler.hpp>
#include <algorithm>
#include <list>

namespace met::detail {
  void LinearScheduler::register_task(const KeyType &prev, TaskType &&task) {
    // Parse task info object by consuming task
    TaskInitInfo info(_resource_registry, *task.get());
    
    // Update resource registry; add/remove task resources
    auto &local_registry = _resource_registry[task->name()];
    local_registry.merge(info.add_resource_registry);
    for (auto &key : info.remove_resource_registry) { local_registry.erase(key); }
      
    // Move task into registry
    if (prev.empty()) {
      // No previous task key provided; insert at end of list
      _task_registry.emplace_back(std::move(task));
    } else {
      // Previous task key provided insert after said task
      auto i = std::ranges::find_if(_task_registry, [prev](const auto &t) { return t->name() == prev; });
      _task_registry.emplace(i == _task_registry.end() ? i : i + 1, std::move(task));
    }

    // Update task registry; add+register/remove subtasks
    for (auto &[key, task] : info.add_task_registry) { register_task(key, std::move(task)); }
    for (auto &key : info.remove_task_registry)      { remove_task(key); }
  }
  
  void LinearScheduler::deregister_task(TaskType &task) {
    // Parse task info object by consuming task
    TaskDstrInfo info(_resource_registry, *task.get());

    // Update resource registry; remove task resources
    _resource_registry.erase(task->name());

    // Update task registry; add+register/remove subtasks
    for (auto &[key, task] : info.add_task_registry) { register_task(key, std::move(task)); }
    for (auto &key : info.remove_task_registry)      { remove_task(key); }
  }

  void LinearScheduler::run() {
    std::list<std::pair<KeyType, TaskType>> add_task_registry;
    std::list<KeyType>                      remove_task_registry;

    // Run all tasks in vector inserted order
    for (auto &task : _task_registry) {
      // Parse task info object by consuming task::eval()
      TaskEvalInfo info(_resource_registry, *task.get());

      // Process added/removed resources **immediately** after task execution
      auto &local_registry = _resource_registry[task->name()];
      local_registry.merge(std::move(info.add_resource_registry));
      for (auto &key : info.remove_resource_registry) { local_registry.erase(key); }

      // Defer added/removed task update until after all tasks have completed
      add_task_registry.merge(std::move(info.add_task_registry));
      remove_task_registry.merge(std::move(info.remove_task_registry));
    }

    // Update task registry; add+register/remove subtasks
    for (auto &[key, task] : add_task_registry) { register_task(key, std::move(task)); }
    for (auto &key : remove_task_registry)      { remove_task(key); }
  }
  
  void LinearScheduler::remove_task(const KeyType &key) {
    std::erase_if(_task_registry, [&](auto &task) { 
      if (task->name() == key) {
        deregister_task(task);
        return true;
      }
      return false;
    });
  }
  
  void LinearScheduler::erase_resource(const KeyType &key) {
    _resource_registry["global"].erase(key);
  }
} // namespace met::detail