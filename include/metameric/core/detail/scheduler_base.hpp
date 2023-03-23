#pragma once

#include <metameric/core/utility.hpp>
#include <memory>
#include <unordered_map>

namespace met {
  // Forward declarations
  class SchedulerBase;
  class SchedulerHandle;

  namespace detail {
    enum class StateFlag { eFresh, eModified };

    // Virtual base class for tasks submitted to application scheduler
    // Implementations contain majority of program code
    struct TaskBase {
      // Override and implement; setup of task
      virtual void init(SchedulerHandle &) { };

      // Override and implement; main body of task
      virtual void eval(SchedulerHandle &) { };

      // Override and implement; teardown of task
      virtual void dstr(SchedulerHandle &) { };

      // Override and implement; on false return, eval(...) is not called
      virtual bool eval_state(SchedulerHandle &) { return true; }
    };

    // Abstract base class for application resources;
    template <typename> struct RsrcImpl; // FWD of container class
    struct RsrcBase {
    private:
      // Internal state flags for resource data; default is modified
      enum class StateFlag { eFresh, eModified } m_state = StateFlag::eModified;

    public:
      // Non-const accessor of underlying data; flag as modified
      template <typename T>
      T & get() {
        set_modify();
        return static_cast<RsrcImpl<T> *>(this)->m_object;
      }

      // Const accessor of underlying data
      template <typename T>
      const T & get() const {
        return static_cast<const RsrcImpl<T> *>(this)->m_object;
      }

      // State operands/queries; the resource is either modified, or not
      void set_modify()        { m_state = StateFlag::eModified; }
      void clear_modify()      { m_state = StateFlag::eFresh; }
      bool is_modified() const { return m_state == StateFlag::eModified; }
    };

    // Implementation class for application resource to hold specific type
    template <typename T>
    struct RsrcImpl : RsrcBase {
      T m_object;

      RsrcImpl(T &&object)
      : m_object(std::move(object)) { }
    };

    // Shorthands for shared_ptr wrapper
    using TaskNode = std::shared_ptr<TaskBase>;
    using RsrcNode = std::shared_ptr<RsrcBase>;

    class SchedulerBase {
    protected:
      // Used internally; implement as "global" for scheduler, and task key for schedule handle
      virtual const std::string &task_default_key() const = 0;

      // Internal parameter objects for adding/removing/accessing tasks/resources in a schedule
      struct TaskInfo { std::string prnt_key = "", task_key = ""; TaskNode ptr = nullptr; };
      struct RsrcInfo { std::string task_key = "", rsrc_key = ""; RsrcNode ptr = nullptr; };

    public: /* Virtual implementation functions for add/remove/get of tasks/resources */
      virtual void      add_task_impl(TaskInfo &&)       = 0;
      virtual TaskBase *get_task_impl(TaskInfo &&) const = 0; // nullable return value
      virtual void      rem_task_impl(TaskInfo &&)       = 0;
      virtual RsrcBase *add_rsrc_impl(RsrcInfo &&)       = 0; // nullable return value
      virtual RsrcBase *get_rsrc_impl(RsrcInfo &&) const = 0; // nullable return value
      virtual void      rem_rsrc_impl(RsrcInfo &&)       = 0;
      
    public: /* Add/remove/get task functions */
      template <typename Ty, typename... Args>
      void emplace_task(const std::string &key, Args... args) {
        static_assert(std::is_base_of_v<TaskBase, Ty>);
        met_trace();
        add_task_impl({ .task_key = key, .ptr = std::make_shared<Ty>(args...) });
      }

      template <typename Ty>
      void insert_task(const std::string &key, Ty &&task) {
        static_assert(std::is_base_of_v<TaskBase, Ty>);
        met_trace();
        add_task_impl({ .task_key = key, .ptr = std::make_shared<Ty>(std::move(task)) });
      }

      void remove_task(const std::string &key) {
        met_trace();
        rem_task_impl({ .task_key = key });
      }

      template <typename Ty>
      const Ty & task(const std::string &key) const {
        met_trace();
        const TaskBase *ptr = get_task_impl({ .task_key = key });
        debug::check_expr_rel(ptr, fmt::format("task failed for {}", key));
        return *static_cast<Ty *>(ptr);
      };

      template <typename Ty>
      Ty & task(const std::string &key) {
        met_trace();
        TaskBase *ptr = get_task_impl({ .task_key = key });
        debug::check_expr_rel(ptr, fmt::format("task failed for {}", key));
        return *static_cast<Ty *>(ptr);
      };
      
      bool has_task(const std::string &key) const {
        met_trace();
        return get_task_impl({ .task_key = key }) != nullptr;
      }

    public: /* Add/remove/get resource functions */
      template <typename Ty, typename InfoTy = Ty::InfoType>
      Ty & emplace_resource(const std::string &key, InfoTy info) {
        met_trace();
        RsrcBase *ptr = add_rsrc_impl({ .task_key = task_default_key(), 
                                        .rsrc_key = key, 
                                        .ptr      = std::make_shared<detail::RsrcImpl<Ty>>(Ty(info)) });
        return ptr->get<Ty>();
      }

      template <typename Ty>
      void insert_resource(const std::string &key, Ty &&rsrc) {
        met_trace();
        RsrcBase *ptr = add_rsrc_impl({ .task_key = task_default_key(), 
                                        .rsrc_key = key, 
                                        .ptr      = std::make_shared<detail::RsrcImpl<Ty>>(std::move(rsrc)) });
      }

      void remove_resource(const std::string &key) {
        met_trace();
        rem_rsrc_impl({ .task_key = task_default_key(), .rsrc_key = key });
      }

      template <typename Ty>
      const Ty & resource(const std::string &task_key, const std::string &rsrc_key) const {
        met_trace();
        const RsrcBase *ptr = get_rsrc_impl({ .task_key = task_key, .rsrc_key = rsrc_key });
        debug::check_expr_rel(ptr, fmt::format("use_resource failed for {}, {}", task_key, rsrc_key));
        return ptr->get<Ty>();
      }

      template <typename Ty>
      Ty & use_resource(const std::string &task_key, const std::string &rsrc_key) {
        met_trace();
        RsrcBase *ptr = get_rsrc_impl({ .task_key = task_key, .rsrc_key = rsrc_key});
        debug::check_expr_rel(ptr, fmt::format("use_resource failed for {}, {}", task_key, rsrc_key));
        return ptr->get<Ty>();
      }

      bool is_resource_modified(const std::string &task_key, const std::string &key) const {
        met_trace();
        return get_rsrc_impl({ .task_key = task_key, .rsrc_key = key })->is_modified();
      }
      
      bool has_resource(const std::string &task_key, const std::string &rsrc_key) const {
        met_trace();
        return get_rsrc_impl({ .task_key = task_key, .rsrc_key = rsrc_key }) != nullptr;
      }

    public: /* Miscellaneous functions */
      // Rebuild underlying schedule from current list of submitted tasks/resources
      virtual void build() = 0;

      // Clear out tasks and owned resources; preserve_global t/f -> retain non-owned resources
      virtual void clear(bool preserve_global = true) = 0;

      // Debug output string formatted task schedule
      virtual std::vector<std::string> schedule() const = 0;
      using RsrcMap = std::unordered_map<std::string, std::unordered_map<std::string, RsrcNode>>;
      virtual const RsrcMap &resources() const = 0;
    };
  } // namespace detail

  // String key referring to global, non-owned resources in scheduler resource management
  const std::string global_key = "global";

  // Virtual base class for application sscheduler
  struct SchedulerBase : public detail::SchedulerBase {
  protected:
    virtual const std::string &task_default_key() const { return global_key; }
    virtual void run_clear_state_impl() = 0; // Used internally; set all resources to 'unmodified'
    virtual void run_schedule_impl()    = 0; // Used internally; run all tasks in order

  public:
    // Run the currently built schedule
    void run() {
      run_clear_state_impl();
      run_schedule_impl();
    };
  };

  // Virtual base class for scheduler handle, passed to task nodes
  struct SchedulerHandle : public detail::SchedulerBase {
  protected:
    virtual const std::string &task_default_key() const { return task_key(); }

  public:
    // Get key of the current active task
    virtual const std::string &task_key() const = 0;

  public: /* Add/remove/get local subtask functions */
    template <typename Ty, typename... Args>
    void emplace_subtask(const std::string &key, Args... args) {
      static_assert(std::is_base_of_v<detail::TaskBase, Ty>);
      met_trace();
      add_task_impl({ .prnt_key = task_default_key(), .task_key = key, .ptr = std::make_shared<Ty>(args...) });
    }

    template <typename Ty>
    void insert_subtask(const std::string &key, Ty &&task) {
      static_assert(std::is_base_of_v<detail::TaskBase, Ty>);
      met_trace();
      add_task_impl({ .prnt_key = task_default_key(), .task_key = key, .ptr = std::make_shared<Ty>(std::move(task)) });
    }

    void remove_subtask(const std::string &key) {
      met_trace();
      rem_task_impl({ .prnt_key = task_default_key(), .task_key = key });
    }
    
    template <typename Ty>
    const Ty & subtask(const std::string &key) const {
      met_trace();
      const detail::TaskBase *ptr = get_task_impl({ .prnt_key = task_default_key(), .task_key = key });
      debug::check_expr_rel(ptr, fmt::format("task failed for {}.{}", task_default_key(), key));
      return *static_cast<Ty *>(ptr);
    };
    
    template <typename Ty>
    Ty & subtask(const std::string &key) {
      met_trace();
      detail::TaskBase *ptr = get_task_impl({ .prnt_key = task_default_key(), .task_key = key });
      debug::check_expr_rel(ptr, fmt::format("task failed for {}.{}", task_default_key(), key));
      return *static_cast<Ty *>(ptr);
    };
    
    bool has_subtask(const std::string &key) const {
      met_trace();
      return get_task_impl({ .prnt_key = task_default_key(), .task_key = key }) != nullptr;
    }

  public: /* Add/remove/get local resource functions */
    // Ensure parent method names are available
    using detail::SchedulerBase::resource;
    using detail::SchedulerBase::use_resource;
    using detail::SchedulerBase::is_resource_modified;
    using detail::SchedulerBase::has_resource;

    template <typename Ty>
    const Ty & resource(const std::string &key) const {
      met_trace();
      const detail::RsrcBase *ptr = get_rsrc_impl({ .task_key = task_default_key(), .rsrc_key = key });
      debug::check_expr_rel(ptr, fmt::format("use_resource failed for {}, {}", task_default_key(), key));
      return ptr->get<Ty>();
    }

    template <typename Ty>
    Ty & use_resource(const std::string &key) {
      met_trace();
      detail::RsrcBase *ptr = get_rsrc_impl({ .task_key = task_default_key(), .rsrc_key = key });
      debug::check_expr_rel(ptr, fmt::format("use_resource failed for {}, {}", task_default_key(), key));
      return ptr->get<Ty>();
    }

    bool is_resource_modified(const std::string &key) const {
      met_trace();
      return get_rsrc_impl({ .task_key = task_default_key(), .rsrc_key = key })->is_modified();
    }
    
    bool has_resource(const std::string &key) const {
      met_trace();
      return get_rsrc_impl({ .task_key = task_default_key(), .rsrc_key = key }) != nullptr;
    }
  };
} // namespace met::detail