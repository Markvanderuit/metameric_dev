#pragma once

#include <metameric/core/detail/scheduler_resource.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <memory>
#include <span>
#include <sstream>
#include <string>

namespace met {
  class LinearScheduler {
    using KeyType  = std::string;
    using RsrcType = std::shared_ptr<detail::AbstractResource>;
    using TaskType = std::shared_ptr<detail::AbstractTask>;
    
    std::unordered_map<KeyType, std::unordered_map<KeyType, RsrcType>> _rsrc_registry;
    std::vector<TaskType>                                              _task_registry;

    void register_task(const KeyType &prev, TaskType &&task);
    void deregister_task(const KeyType &key);

  public:

    /* scheduling */

    void run();

    /* Create, add, remove tasks */
    
    template <typename Ty, typename... Args>
    void emplace_task(const KeyType &key, Args... args) {
      static_assert(std::is_base_of_v<detail::AbstractTask, Ty>);
      register_task("", std::make_shared<Ty>(key, args...));
    }

    template <typename Ty, typename... Args>
    void emplace_task_after(const KeyType &prev, const KeyType &key, Args... args) {
      static_assert(std::is_base_of_v<detail::AbstractTask, Ty>);
      register_task(prev, std::make_shared<Ty>(key, args...));
    }

    template <typename Ty>
    void insert_task(Ty &&task) {
      static_assert(std::is_base_of_v<detail::AbstractTask, Ty>);
      register_task("", std::make_shared<Ty>(std::move(task)));
    }

    template <typename Ty>
    void insert_task_after(const KeyType &prev, Ty &&task) {
      static_assert(std::is_base_of_v<detail::AbstractTask, Ty>);
      register_task(prev, std::make_shared<Ty>(std::move(task)));
    }

    void remove_task(const KeyType &key);

    /* create/add/remove global resources */

    template <typename Ty, typename InfoTy = Ty::InfoType>
    Ty& emplace_resource(const KeyType &key, InfoTy info) {
      using ResourceType = detail::Resource<Ty>;
      auto [it, r] = _rsrc_registry[global_key].emplace(key, std::make_shared<ResourceType>(Ty(info)));
      debug::check_expr_dbg(r, fmt::format("could not emplace resource with key: {}", key));
      return it->second->get_as<Ty>();
    }
  
    template <typename Ty>
    void insert_resource(const KeyType &key, Ty &&rsrc) {
      using ResourceType = detail::Resource<Ty>;
      auto [it, r] = _rsrc_registry[global_key].emplace(key, std::make_shared<ResourceType>(std::move(rsrc)));
      debug::check_expr_dbg(r, fmt::format("could not insert resource with key: {}", key));
    }

    void remove_resource(const KeyType &key);

    /* Access existing resources */
    
    template <typename T>
    T & get_resource(const KeyType &task_key, const KeyType &rsrc_key) {
      return _rsrc_registry.at(task_key).at(rsrc_key)->get_as<T>();
    }

    /* miscellaneous, dbeug info */

    void clear_tasks();  // Clear tasks and owned resources; retain global resources
    void clear_global(); // Clear global resources
    void clear_all();    // Clear tasks and resources 

    // Return const list of current tasks
    const std::vector<TaskType>& tasks() const {
      return _task_registry;
    }
    
    // String output of current task schedule
    std::vector<std::string> schedule_list() const {
      std::vector<std::string> v(_task_registry.size());
      std::ranges::transform(_task_registry, v.begin(), [](const auto &task) { return task->name(); });
      return v;
    }
  };
} // namespace met