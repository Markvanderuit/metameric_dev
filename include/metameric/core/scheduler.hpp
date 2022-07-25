#pragma once

#include <metameric/core/detail/scheduler_resource.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <memory>
#include <span>
#include <string>

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

    template <typename Ty, typename InfoTy = Ty::InfoType>
    Ty& emplace_resource(const KeyType &key, InfoTy info) {
      _resource_registry[global_key].emplace(key, 
        std::make_shared<detail::Resource<Ty>>(Ty(info)));
      return _resource_registry.at(global_key).at(key)->get_as<Ty>();
    }
  
    template <typename Ty>
    void insert_resource(const KeyType &key, Ty &&rsrc) {
      _resource_registry[global_key].emplace(key, 
        std::make_shared<detail::Resource<Ty>>(std::move(rsrc)));
    }

    void erase_resource(const KeyType &key);

    /* Access existing resources */
    
    template <typename T>
    T & get_resource(const KeyType &task_key, const KeyType &rsrc_key) {
      return _resource_registry.at(task_key).at(rsrc_key)->get_as<T>();
    }

    /* miscellaneous */

    // Clear tasks and owned resources; retain global resources
    void clear_tasks();

    // Clear tasks and all resources 
    void clear_all();

    std::span<const TaskType> tasks() const {
      return _task_registry;
    }
  };
} // namespace met