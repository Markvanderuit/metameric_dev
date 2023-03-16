#pragma once

#include <metameric/core/detail/scheduler_info.hpp>
#include <memory>
#include <unordered_map>
#include <utility>

namespace met::detail {
  // Forward declarations
  struct TaskBase;
  struct RsrcBase;
  template <typename> 
  struct RsrcImpl;
  class TaskInfo;

  // Shorthands for common types
  using TaskNode = std::shared_ptr<TaskBase>;
  using RsrcNode = std::shared_ptr<RsrcBase>;
  using TaskMap  = std::unordered_map<std::string, TaskNode>;
  using RsrcMap  = std::unordered_map<std::string, std::unordered_map<std::string, RsrcNode>>;

  // Abstract base class for application tasks;
  // Implementations contain majority of program code
  struct TaskBase {
    // Override and implement
    virtual void init(TaskInfo &) { };
    virtual void eval(TaskInfo &) = 0;
    virtual void dstr(TaskInfo &) { };
  };

  // Abstract base class for application resources;
  // Implementation described below
  struct RsrcBase {
    template <typename T>
    T & get_as() {
      return static_cast<RsrcImpl<T> *>(this)->m_object;
    }

    template <typename T>
    const T & get_as() const {
      return static_cast<RsrcImpl<T> *>(this)->m_object;
    }
  };

  // Implementation class for application resource to hold specific type
  template <typename T>
  struct RsrcImpl : RsrcBase {
    T m_object;

    RsrcImpl(T &&object)
    : m_object(std::move(object)) { }
  };

  class TaskSchedulerBase {
    virtual void add_task_impl(AddTaskInfo &&) = 0;
    virtual void rem_task_impl(RemTaskInfo &&) = 0;
    
  public:
    template <typename Ty, typename... Args>
    void emplace_task(const std::string &key, Args... args) {
      static_assert(std::is_base_of_v<TaskBase, Ty>);
      met_trace();
      add_task_impl(AddTaskInfo { .prev_key = "",
                                  .task_key = key,
                                  .task     = std::make_shared<Ty>(args...)});
    }

    template <typename Ty, typename... Args>
    void emplace_task_after(const std::string &prev, const std::string &key, Args... args) {
      static_assert(std::is_base_of_v<detail::TaskBase, Ty>);
      met_trace();
      add_task_impl(AddTaskInfo { .prev_key = prev,
                                  .task_key = key,
                                  .task     = std::make_shared<Ty>(args...)});
    }

    template <typename Ty>
    void insert_task(const std::string &key, Ty &&task) {
      static_assert(std::is_base_of_v<detail::TaskBase, Ty>);
      met_trace();
      add_task_impl(AddTaskInfo { .prev_key = "",
                                  .task_key = key,
                                  .task = std::make_shared<Ty>(std::move(task)) });
    }

    template <typename Ty>
    void insert_task_after(const std::string &prev, const std::string &key, Ty &&task) {
      static_assert(std::is_base_of_v<detail::TaskBase, Ty>);
      met_trace();
      add_task_impl(AddTaskInfo { .prev_key = prev,
                                  .task_key = key,
                                  .task = std::make_shared<Ty>(std::move(task)) });
    }

    void remove_task(const std::string &key) {
      met_trace();
      rem_task_impl(RemTaskInfo { .task_key = key });
    }
  };
  
  class RsrcSchedulerBase {
    virtual RsrcNode add_rsrc_impl(AddRsrcInfo &&) = 0;
    virtual RsrcNode get_rsrc_impl(GetRsrcInfo &&) = 0;
    virtual void     rem_rsrc_impl(RemRsrcInfo &&) = 0;
    virtual const std::string &task_key_impl() const = 0;

  public:
    template <typename Ty, typename InfoTy = Ty::InfoType>
    Ty& emplace_resource(const std::string &key, InfoTy info) {
      met_trace();
      auto node = add_rsrc_impl(AddRsrcInfo { .task_key = task_key_impl(),
                                              .rsrc_key = key,
                                              .rsrc     = std::make_shared<detail::RsrcImpl<Ty>>(Ty(info)) });
      return node->get_as<Ty>();
    }

    template <typename Ty>
    void insert_resource(const std::string &key, Ty &&rsrc) {
      met_trace();
      add_rsrc_impl(AddRsrcInfo { .task_key = task_key_impl(),
                                  .rsrc_key = key,
                                  .rsrc     = std::make_shared<detail::RsrcImpl<Ty>>(std::move(rsrc)) });
    }

    void remove_resource(const std::string &key) {
      met_trace();
      rem_rsrc_impl(RemRsrcInfo { .task_key = task_key_impl(), .rsrc_key = key });
    }

    template <typename Ty>
    const Ty & get_resource(const std::string &key) const {
      met_trace();
      return get_rsrc_impl(GetRsrcInfo { .task_key = task_key_impl(), .rsrc_key = key })->get_as<Ty>();
    }

    template <typename Ty>
    Ty & get_resource(const std::string &key) {
      met_trace();
      return get_rsrc_impl(GetRsrcInfo { .task_key = task_key_impl(), .rsrc_key = key })->get_as<Ty>();
    }

    template <typename Ty>
    const Ty & get_resource(const std::string &task_key, const std::string &rsrc_key) const {
      met_trace();
      return get_rsrc_impl(GetRsrcInfo { .task_key = task_key, .rsrc_key = rsrc_key })->get_as<Ty>();
    }

    template <typename Ty>
    Ty & get_resource(const std::string &task_key, const std::string &rsrc_key) {
      met_trace();
      return get_rsrc_impl(GetRsrcInfo { .task_key = task_key, .rsrc_key = rsrc_key })->get_as<Ty>();
    }

    bool has_resource(const std::string &task_key, const std::string &rsrc_key) {
      met_trace();
      return get_rsrc_impl(GetRsrcInfo { .task_key = task_key, .rsrc_key = rsrc_key }) != nullptr;
    }
  };
} // namespace met::detail