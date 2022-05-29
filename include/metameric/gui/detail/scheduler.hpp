#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <metameric/gui/detail/linear_scheduler/task.hpp>
#include <metameric/gui/detail/linear_scheduler/resource.hpp>

namespace met::detail {
  class LinearScheduler {
    using KeyType           = std::string;
    using RsrcType          = std::unique_ptr<AbstractResource>;
    using TaskType          = std::unique_ptr<AbstractTask>;
    
    std::unordered_map<KeyType, 
                        std::unordered_map<KeyType, RsrcType>> 
                            _resource_registry;
    std::vector<TaskType>   _task_registry;

    void register_task(const KeyType &prev, TaskType &&task) {
      // Parse task info object by consuming task
      TaskInitInfo init_info(_resource_registry, *task.get());
      
      // Merge resources into registry
      _resource_registry[task->name()].merge(init_info.add_resource_registry);

      // Remove flagged resources from registry
      for (auto &key : init_info.remove_resource_registry) {
        _resource_registry[task->name()].erase(key);
      }
      
      // Move task into registry
      if (prev.empty()) {
        _task_registry.emplace_back(std::move(task));
      } else {
        auto iter = std::ranges::find_if(_task_registry, 
          [prev](const auto &task) { return task->name() == prev; });
        if (iter != _task_registry.end()) {
          iter++;
        }
        _task_registry.emplace(iter, std::move(task));
      }
      
      // Recursively register added subtasks
      for (auto &[prev, task]: init_info.add_task_registry)
        register_task(prev, std::move(task));

      // Remove flagged tasks from registry
      for (auto &key : init_info.remove_task_registry) {
        remove_task(key);
      }
    }
    
  public:

    /* Create, add, remove tasks */
    
    template <typename Ty, typename... Args>
    void emplace_task(const KeyType &key, Args... args) {
      static_assert(std::is_base_of_v<AbstractTask, Ty>);
      register_task("", std::make_unique<Ty>(key, args...));
    }

    template <typename Ty>
    void insert_task(const KeyType &key, Ty &&task) {
      static_assert(std::is_base_of_v<AbstractTask, Ty>);
      register_task("", std::make_unique<Ty>(std::move(task)));
    }
    
    template <typename Ty, typename... Args>
    void emplace_task_after(const KeyType &prev, const KeyType &key, Args... args) {
      static_assert(std::is_base_of_v<AbstractTask, Ty>);
      register_task(prev, std::make_unique<Ty>(key, args...));
    }

    template <typename Ty>
    void insert_task_after(const KeyType &prev, const KeyType &key, Ty &&task) {
      static_assert(std::is_base_of_v<AbstractTask, Ty>);
      register_task(prev, std::make_unique<Ty>(std::move(task)));
    }

    void remove_task(const KeyType &key) {
      std::erase_if(_task_registry, [key](auto &task) { return task->name() == key; });
      _resource_registry.erase(key);
    }

    /* create/add/remove global resources */

    template <typename Ty, typename InfoTy>
    Ty& emplace_resource(const KeyType &key, InfoTy info) {
      _resource_registry["global"].emplace(key, std::make_unique<detail::Resource<Ty>>(Ty(info)));
      return _resource_registry.at("global").at(key)->get<Ty>();
    }
  
    template <typename Ty>
    void insert_resource(const KeyType &key, Ty &&rsrc) {
      _resource_registry["global"].emplace(key, std::make_unique<detail::Resource<Ty>>(std::move(rsrc)));
    }

    void remove_resource(const KeyType &key) {
      _resource_registry["global"].erase(key);
    }

    /* scheduling */

    void run() {
      std::list<std::pair<KeyType, TaskType>> add_task_registry;
      std::list<KeyType>                      remove_task_registry;

      // Run all tasks in vector inserted order
      for (auto &task : _task_registry) {
        // Parse task info object by consuming task::eval()
        TaskEvalInfo eval_info(_resource_registry, *task.get());

        // Obtain registry for current task
        auto &registry = _resource_registry.at(task->name());

        // Process added/removed resources after task execution
        registry.merge(std::move(eval_info.add_resource_registry));
        for (auto &key : eval_info.remove_resource_registry) {
          registry.erase(key);          
        }

        // Defer added/removed task update until after tasks have completed
        add_task_registry.merge(std::move(eval_info.add_task_registry));
        remove_task_registry.merge(std::move(eval_info.remove_task_registry));
      }

      // Register added tasks
      for (auto &[prev, task] : add_task_registry) {
        register_task(prev, std::move(task));
      }
      
      // Remove flagged tasks from registry
      for (auto &key : remove_task_registry) {
        remove_task(key);
      }
    }
  };
} // met::detail