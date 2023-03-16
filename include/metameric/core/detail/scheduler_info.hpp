#pragma once

#include <memory>
#include <string>

namespace met::detail {
  // Forward declaration
  struct TaskBase;
  struct RsrcBase;

  // Pointer wrappers
  using TaskNode = std::shared_ptr<TaskBase>;
  using RsrcNode = std::shared_ptr<RsrcBase>;

  // Info object for adding a new task to a schedule
  struct AddTaskInfo {
    std::string prev_key = "";      // Key of previous task after which task is appended
    std::string task_key = "";      // Key given to task
    TaskNode    task     = nullptr; // Pointer to task
  };

  // Info object for removing a task from the schedule
  struct RemTaskInfo {
    std::string task_key = "";
  };

  // Info boject for adding a new resource to a task
  struct AddRsrcInfo {
    std::string task_key = "";
    std::string rsrc_key = "";
    RsrcNode    rsrc     = nullptr; // Pointer to resource
  };

  // Info object for removing a resource from a task 
  struct RemRsrcInfo {
    std::string task_key = "";
    std::string rsrc_key = "";
  };

  // Info object for querying a resource from a task 
  struct GetRsrcInfo {
    std::string task_key = "";
    std::string rsrc_key = "";
  };
} // namespace detail