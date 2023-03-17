#pragma once

#include <memory>
#include <string>

namespace met::detail {
  // Forward declaration
  struct TaskBase;
  struct RsrcBase;

  // Pointer wrappers
  using TaskNode = std::unique_ptr<TaskBase>;
  using RsrcNode = std::unique_ptr<RsrcBase>;

  // Info object for adding a new task to a schedule
  struct AddTaskInfo {
    std::string prnt_key = "";      // Key of parent task to which task is appended
    std::string task_key = "";      // Key of task
    TaskNode    task     = nullptr; // Pointer to task
  };

  // Info object for removing a task from the schedule
  struct RemTaskInfo {
    std::string prnt_key = "";      // Key of parent task to which task is appended
    std::string task_key = "";      // Key of task
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