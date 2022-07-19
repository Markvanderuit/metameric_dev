#pragma once

#include <metameric/core/detail/scheduler_resource.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <memory>
#include <span>
#include <string>

#include <iostream>

namespace met {
  class LinearScheduler {
    using KeyType  = std::string;
    using RsrcType = std::shared_ptr<detail::AbstractResource>;
    using TaskType = std::shared_ptr<detail::AbstractTask>;
    
    std::unordered_map<KeyType, std::unordered_map<KeyType, RsrcType>> _resource_registry;
    std::vector<TaskType>                                              _task_registry;

    void register_task(const KeyType &prev, TaskType &&task);
    void deregister_task(TaskType &task);

  public:

    /* scheduling */

    void run();

    /* Create, add, remove tasks */
    
    template <typename Ty, typename... Args>
    void emplace_task(const KeyType &key, Args... args) {
      static_assert(std::is_base_of_v<detail::AbstractTask, Ty>);
      register_task("", std::make_shared<Ty>(key, args...));
    }

    template <typename Ty>
    void insert_task(const KeyType &key, Ty &&task) {
      static_assert(std::is_base_of_v<detail::AbstractTask, Ty>);
      register_task("", std::make_shared<Ty>(std::move(task)));
    }
    
    template <typename Ty, typename... Args>
    void emplace_task_after(const KeyType &prev, const KeyType &key, Args... args) {
      static_assert(std::is_base_of_v<detail::AbstractTask, Ty>);
      register_task(prev, std::make_shared<Ty>(key, args...));
    }

    template <typename Ty>
    void insert_task_after(const KeyType &prev, const KeyType &key, Ty &&task) {
      static_assert(std::is_base_of_v<detail::AbstractTask, Ty>);
      register_task(prev, std::make_shared<Ty>(std::move(task)));
    }

    void remove_task(const KeyType &key);

    /* create/add/remove global resources */

    template <typename Ty, typename InfoTy>
    Ty& emplace_resource(const KeyType &key, InfoTy info) {
      _resource_registry[detail::resource_global_key].emplace(key, 
        std::make_shared<detail::Resource<Ty>>(Ty(info)));
      return _resource_registry.at(detail::resource_global_key).at(key)->get_as<Ty>();
    }
  
    template <typename Ty>
    void insert_resource(const KeyType &key, Ty &&rsrc) {
      _resource_registry[detail::resource_global_key].emplace(key, 
        std::make_shared<detail::Resource<Ty>>(std::move(rsrc)));
    }

    void erase_resource(const KeyType &key);

    /* miscellaneous */

    void clear() {
      _resource_registry.clear();
      _task_registry.clear();
    }

    std::span<const TaskType> tasks() const {
      return _task_registry;
    }
  };
} // namespace met