#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <fmt/core.h>
#include <metameric/gui/detail/resources.hpp>
#include <metameric/gui/detail/tasks.hpp>
#include <metameric/gui/detail/graph.hpp>

namespace met {
  struct AbstractResourceNode; // fwd
  struct AbstractTaskNode; // fwd

  struct AbstractTaskNode {
    using PtrType = AbstractTask *;

    PtrType _task = nullptr;

  public:
    AbstractTaskNode() = default;
    AbstractTaskNode(const PtrType task)
    : _task(task)
    { }

    std::vector<AbstractResourceNode *> prev, next;
  };

  class AbstractResourceNode {
    using PtrType = std::shared_ptr<detail::VirtualResource>;

    PtrType _resource = nullptr;

  public:
    AbstractResourceNode() = default;
    AbstractResourceNode(const PtrType &resource)
    : _resource(resource)
    { }

    template <typename T>
    T & value_as() {
      return _resource->value_as<T>;
    }

    std::vector<AbstractTask *> prev, next;
  };

  class ApplicationScheduler {
    using KeyType       = std::string;
    using TaskPtrType   = std::unique_ptr<AbstractTask>;
    using RsrcPtrType   = std::shared_ptr<detail::VirtualResource>;
    using PairType      = std::pair<KeyType, TaskPtrType>;
    using CreateHandle  = std::pair<KeyType, RsrcPtrType>;
    using ReadHandle    = std::pair<KeyType, KeyType>;
    using WriteHandle   = std::pair<ReadHandle, KeyType>;

    std::unordered_map<KeyType, ResourceHolder> _resources_registry;
    ApplicationTasks                            _tasks_registry;

  public:
    template <typename Ty, typename... Args>
    void create_task(Args... args) {
      insert_task<Ty>(std::move(Ty(args...)));
    }
    
    template <typename Ty>
    void insert_task(Ty &&task) {
      static_assert(std::is_base_of_v<AbstractTask, Ty>, 
                    "ApplicationScheduler::insert_task<Ty>(...);\
                     Ty must derive from AbstractTask");

      // Initialize task info object through ApplicationTask::create(...)
      CreateTaskInfo info(task);

      // Create task resources recorded in info object
      ResourceHolder resource_holder;
      std::ranges::for_each(info.resources, [&resource_holder](CreateHandle &handle) {
        resource_holder.resources.insert(handle.first, std::move(handle.second));
      });
      resource_holder.reads = std::move(info.reads);
      resource_holder.writes = std::move(info.writes);

      // Insert task and accompanying resources into registries
      _resources_registry.insert({ task.name(), std::move(resource_holder) });
      _tasks_registry.insert(std::move(task));
    }

    void remove_task(const std::string &name) {
      _tasks_registry.erase(name);
      _resources_registry.erase(name);
    }

    void compile() {
      return;

      std::vector<AbstractTaskNode> task_nodes;
      std::vector<AbstractResourceNode> resource_nodes;
      
      std::ranges::for_each(_tasks_registry.data(), [](auto &task) {
        
      });

      std::ranges::for_each(_resources_registry, [](auto &pair) {
        const KeyType &name = pair.first;
        ResourceHolder &holder = pair.second;

      });

      /* first part; resolve computation order */
      
      
      /* second part; resolve resource handles */

      // ...
    }

    void run() {
      std::list<PairType> inserts;
      std::list<KeyType> erases;

      // Process current active list of tasks
      for (auto &task : _tasks_registry.data()) {
        RuntimeTaskInfo scheduler(_resources_registry, task);

        task->run(scheduler);

        /* erases.merge(std::move(scheduler.erases));
        inserts.merge(std::move(scheduler.inserts)); */
      }

      // Update list of tasks
      /* for (auto &name : erases) { remove_task(name); }
      for (auto &pair : inserts) {
        auto &[prev_name, task] = pair;

        // Consume task to gather task information
        CreateTaskInfo info(*task.get());
        
        // Initialize accompanying resources based on info
        TaskResources resources;
        std::ranges::for_each(info.resources, [&resources](auto &pair) {
          resources.insert(pair.first, std::move(pair.second));
        });

        // Add task and resources to data holders
        _resources_registry.emplace(task->name(), std::move(resources));

        if (!prev_name.empty()) {
          _tasks_registry.insert_after(prev_name, std::move(task));
        } else {
          _tasks_registry.insert(std::move(task));
        }
      } */
    }

    void output_schedule() {
      // std::string output;
      for (auto &task : _tasks_registry.data()) {
        auto &holder = _resources_registry[task->name()];
        std::string read_out;
        for (auto &read : holder.reads) {
          fmt::format_to(std::back_inserter(read_out), "{} - {}, ", read.first, read.second);
        }
        fmt::print("{}, {}\n", task->name(), read_out);
        // fmt::format_to(std::back_inserter(output), "{}, ", task->name());
      }
      // fmt::print("{}\n", output);
    }
  };
} // namespace met