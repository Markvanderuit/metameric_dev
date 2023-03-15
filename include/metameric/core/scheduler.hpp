#pragma once

#include <metameric/core/detail/scheduler_resource.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <memory>
#include <span>
#include <sstream>
#include <string>

namespace met {
  class LinearScheduler {
    using RsrcType = std::shared_ptr<detail::AbstractResource>;
    using TaskType = std::shared_ptr<detail::AbstractTask>;
    using RsrcRegs = std::unordered_map<
      std::string, 
      std::unordered_map<std::string, RsrcType>
    >;
    using TaskRegs = std::vector<TaskType>;
    
  private: /* private members */
    RsrcRegs m_rsrc_registry;
    TaskRegs m_task_registry;

  private: /* private methods */
    void register_task(const std::string &prev, TaskType &&task);
    void deregister_task(const std::string &key);
    // void register_rsrc(const std::string &key, )

  public: /* public methods */
    /* scheduling */

    void build();
    void run();

    /* Create, add, remove tasks */
    
    template <typename Ty, typename... Args>
    void emplace_task(const std::string &key, Args... args) {
      static_assert(std::is_base_of_v<detail::AbstractTask, Ty>);
      register_task("", std::make_shared<Ty>(key, args...));
    }

    template <typename Ty, typename... Args>
    void emplace_task_after(const std::string &prev, const std::string &key, Args... args) {
      static_assert(std::is_base_of_v<detail::AbstractTask, Ty>);
      register_task(prev, std::make_shared<Ty>(key, args...));
    }

    template <typename Ty>
    void insert_task(Ty &&task) {
      static_assert(std::is_base_of_v<detail::AbstractTask, Ty>);
      register_task("", std::make_shared<Ty>(std::move(task)));
    }

    template <typename Ty>
    void insert_task_after(const std::string &prev, Ty &&task) {
      static_assert(std::is_base_of_v<detail::AbstractTask, Ty>);
      register_task(prev, std::make_shared<Ty>(std::move(task)));
    }

    void remove_task(const std::string &key) {
      met_trace();
      deregister_task(key);
    }

    /* create/add/remove global resources */

    template <typename Ty, typename InfoTy = Ty::InfoType>
    Ty& emplace_resource(const std::string &key, InfoTy info) {
      using ResourceType = detail::Resource<Ty>;
      auto [it, r] = m_rsrc_registry[global_key].emplace(key, std::make_shared<ResourceType>(Ty(info)));
      debug::check_expr_dbg(r, fmt::format("could not emplace resource with key: {}", key));
      return it->second->get_as<Ty>();
    }
  
    template <typename Ty>
    void insert_resource(const std::string &key, Ty &&rsrc) {
      using ResourceType = detail::Resource<Ty>;
      auto [it, r] = m_rsrc_registry[global_key].emplace(key, std::make_shared<ResourceType>(std::move(rsrc)));
      debug::check_expr_dbg(r, fmt::format("could not insert resource with key: {}", key));
    }

    void remove_resource(const std::string &key) {
      met_trace();
      m_rsrc_registry[global_key].erase(key);
    }

    /* Access existing resources */
    
    template <typename T>
    T & get_resource(const std::string &task_key, const std::string &rsrc_key) {
      return m_rsrc_registry.at(task_key).at(rsrc_key)->get_as<T>();
    }

    /* miscellaneous, dbeug info */

    void clear_tasks();  // Clear tasks and owned resources; retain global resources
    void clear_global(); // Clear global resources
    void clear_all();    // Clear tasks and resources 

    // Return const registries
    const TaskRegs& tasks() const { return m_task_registry; }
    const RsrcRegs& resources() const { return m_rsrc_registry; }
  };
} // namespace met