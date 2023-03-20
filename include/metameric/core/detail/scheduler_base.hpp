#pragma once

#include <metameric/core/utility.hpp>
#include <memory>
#include <unordered_map>

namespace met::detail {
  // Forward declarations
  struct TaskBase;
  struct RsrcBase;
  struct SchedulerBase;
  struct SchedulerHandle;

  // Abstract base class for application tasks;
  // Implementations contain majority of program code
  struct TaskBase {
    // Override and implement
    virtual void init(SchedulerHandle &) { };
    virtual void eval(SchedulerHandle &) { };
    virtual void dstr(SchedulerHandle &) { };
  };

  // Abstract base class for application resources;
  template <typename> struct RsrcImpl; // FWD of implementing class
  struct RsrcBase {
    template <typename T>
    T & get() {
      return static_cast<RsrcImpl<T> *>(this)->m_object;
    }

    template <typename T>
    const T & get() const {
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

  // Shorthands for common types
  using TaskNode = std::shared_ptr<TaskBase>;
  using RsrcNode = std::shared_ptr<RsrcBase>;
  using TaskMap  = std::unordered_map<std::string, TaskNode>;
  using RsrcMap  = std::unordered_map<std::string, std::unordered_map<std::string, RsrcNode>>;

  class TaskSchedulerBase {
  protected:
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

    // Virtual functions implementing add/remove tasks
    virtual void add_task_impl(AddTaskInfo &&) = 0;
    virtual void rem_task_impl(RemTaskInfo &&) = 0;
    
  public:
    template <typename Ty, typename... Args>
    void emplace_task(const std::string &key, Args... args) {
      static_assert(std::is_base_of_v<TaskBase, Ty>);
      met_trace();
      add_task_impl(AddTaskInfo { .prnt_key = "",
                                  .task_key = key,
                                  .task     = std::make_shared<Ty>(args...)});
    }

    template <typename Ty, typename... Args>
    void emplace_subtask(const std::string &prnt, const std::string &key, Args... args) {
      static_assert(std::is_base_of_v<detail::TaskBase, Ty>);
      met_trace();
      add_task_impl(AddTaskInfo { .prnt_key = prnt,
                                  .task_key = key,
                                  .task     = std::make_shared<Ty>(args...)});
    }

    template <typename Ty>
    void insert_task(const std::string &key, Ty &&task) {
      static_assert(std::is_base_of_v<detail::TaskBase, Ty>);
      met_trace();
      add_task_impl(AddTaskInfo { .prnt_key = "",
                                  .task_key = key,
                                  .task = std::make_shared<Ty>(std::move(task)) });
    }

    template <typename Ty>
    void insert_subtask(const std::string &prnt, const std::string &key, Ty &&task) {
      static_assert(std::is_base_of_v<detail::TaskBase, Ty>);
      met_trace();
      add_task_impl(AddTaskInfo { .prnt_key = prnt,
                                  .task_key = key,
                                  .task = std::make_shared<Ty>(std::move(task)) });
    }

    void remove_task(const std::string &key) {
      met_trace();
      rem_task_impl(RemTaskInfo { .task_key = key });
    }

    void remove_subtask(const std::string &prnt, const std::string &key) {
      met_trace();
      rem_task_impl(RemTaskInfo { .prnt_key = prnt, .task_key = key });
    }
  };
  
  class RsrcSchedulerBase {
  protected:
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

    // Virtual functions implementing add/remove/get tasks
    virtual RsrcBase* add_rsrc_impl(AddRsrcInfo &&) = 0; // nullable
    virtual RsrcBase* get_rsrc_impl(GetRsrcInfo &&) = 0; // nullable
    virtual void      rem_rsrc_impl(RemRsrcInfo &&) = 0;
    virtual const std::string &task_key_impl() const = 0;

  public:
    template <typename Ty, typename InfoTy = Ty::InfoType>
    Ty& emplace_resource(const std::string &key, InfoTy info) {
      met_trace();
      auto *rsrc = add_rsrc_impl(AddRsrcInfo { .task_key = task_key_impl(),
                                               .rsrc_key = key,
                                               .rsrc     = std::make_unique<detail::RsrcImpl<Ty>>(Ty(info)) });
      return rsrc->get<Ty>();
    }

    template <typename Ty>
    void insert_resource(const std::string &key, Ty &&rsrc) {
      met_trace();
      add_rsrc_impl(AddRsrcInfo { .task_key = task_key_impl(),
                                  .rsrc_key = key,
                                  .rsrc     = std::make_unique<detail::RsrcImpl<Ty>>(std::move(rsrc)) });
    }

    void remove_resource(const std::string &key) {
      met_trace();
      rem_rsrc_impl(RemRsrcInfo { .task_key = task_key_impl(), .rsrc_key = key });
    }

    template <typename Ty>
    const Ty & get_resource(const std::string &key) const {
      met_trace();
      auto ptr = get_rsrc_impl(GetRsrcInfo { .task_key = task_key_impl(), .rsrc_key = key });
      debug::check_expr_rel(ptr, fmt::format("get_resource failed for {}, {}", task_key_impl(), key));
      return ptr->get<Ty>();
    }

    template <typename Ty>
    Ty & get_resource(const std::string &key) {
      met_trace();
      auto ptr = get_rsrc_impl(GetRsrcInfo { .task_key = task_key_impl(), .rsrc_key = key });
      debug::check_expr_rel(ptr, fmt::format("get_resource failed for {}, {}", task_key_impl(), key));
      return ptr->get<Ty>();
    }

    template <typename Ty>
    const Ty & get_resource(const std::string &task_key, const std::string &rsrc_key) const {
      met_trace();
      auto ptr = get_rsrc_impl(GetRsrcInfo { .task_key = task_key, .rsrc_key = rsrc_key });
      debug::check_expr_rel(ptr, fmt::format("get_resource failed for {}, {}", task_key, rsrc_key));
      return ptr->get<Ty>();
    }

    template <typename Ty>
    Ty & get_resource(const std::string &task_key, const std::string &rsrc_key) {
      met_trace();
      auto ptr = get_rsrc_impl(GetRsrcInfo { .task_key = task_key, .rsrc_key = rsrc_key });
      debug::check_expr_rel(ptr, fmt::format("get_resource failed for {}, {}", task_key, rsrc_key));
      return ptr->get<Ty>();
    }
    
    bool has_resource(const std::string &task_key, const std::string &rsrc_key) {
      met_trace();
      return get_rsrc_impl(GetRsrcInfo { .task_key = task_key, .rsrc_key = rsrc_key }) != nullptr;
    }

    const std::string &task_key() const { return task_key_impl(); }
  };

  struct SchedulerBase : public RsrcSchedulerBase,
                         public TaskSchedulerBase {

    // Added debug functions
    virtual std::vector<std::string> schedule() const = 0;
    virtual const detail::RsrcMap &resources() const = 0;
  };

  struct SchedulerHandle : public detail::SchedulerBase {
  protected:
    virtual void signal_clear_tasks_impl() = 0;
    virtual void signal_clear_all_impl()   = 0;

  public:
    void signal_clear_tasks() {
      met_trace();
      signal_clear_tasks_impl();
    }

    void signal_clear_all() {
      met_trace();
      signal_clear_all_impl();
    }
  };
} // namespace met::detail