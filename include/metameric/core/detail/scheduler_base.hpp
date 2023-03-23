#pragma once

#include <metameric/core/utility.hpp>
#include <list>
#include <memory>
#include <unordered_map>

namespace met {
  // Forward declarations
  class SchedulerBase;
  class SchedulerHandle;
  class ResourceHandle;
  class TaskHandle;

  namespace detail {
    class SchedulerBase;
    struct TaskBase;
    struct RsrcBase;

    // Shorthands for shared_ptr wrapper
    using TaskBasePtr = std::shared_ptr<TaskBase>;
    using RsrcBasePtr = std::shared_ptr<RsrcBase>;
  } // namespace detail

  // Internal parameter objects for adding/removing/accessing tasks/resources in a schedule
  struct TaskInfo { std::string prnt_key = "", task_key = ""; detail::TaskBasePtr ptr = nullptr; };
  struct RsrcInfo { std::string task_key = "", rsrc_key = ""; detail::RsrcBasePtr ptr = nullptr; };

  namespace detail {
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
    template <typename> class RsrcImpl; // FWD of container class
    class RsrcBase {
    private:
      bool m_is_mutated;

    public:
      RsrcBase(bool mutated = false) 
      : m_is_mutated(mutated) { }

      // State queries; the resource is either modified, or not
      void set_mutated(bool b) { m_is_mutated = b; }
      bool mutated() const { return m_is_mutated; }

      // Upcast to contained resource object
      template <typename Ty> const Ty & realize() const { return static_cast<const RsrcImpl<Ty> *>(this)->m_object; }
      template <typename Ty>       Ty & realize()       { return static_cast<      RsrcImpl<Ty> *>(this)->m_object; }

      template <typename Ty>
      const Ty & read_only() const {
        met_trace();
        return realize<Ty>();
      }

      template <typename Ty>
      Ty & writeable() {
        met_trace();
        set_mutated(true);
        return realize<Ty>();
      }
    };

    // Implementation class for application resource to hold specific type
    template <typename T>
    class RsrcImpl : public RsrcBase {
      friend class RsrcBase;

      T m_object;

    public:
      RsrcImpl() = default;
      RsrcImpl(T &&object) : RsrcBase(true), m_object(std::move(object)) { }
    };
  } // namespace detail

  namespace detail {
    class SchedulerBase {
    protected:
      // Used internally; implement as "global" for scheduler, and task key for schedule handle
      virtual const std::string &task_default_key() const = 0;

    public: /* Virtual implementation functions for add/remove/get of tasks/resources */
      virtual TaskBase *add_task_impl(TaskInfo &&)       = 0; // nullable return value
      virtual TaskBase *get_task_impl(TaskInfo &&) const = 0; // nullable return value
      virtual void      rem_task_impl(TaskInfo &&)       = 0;
      virtual RsrcBase *add_rsrc_impl(RsrcInfo &&)       = 0; // nullable return value
      virtual RsrcBase *get_rsrc_impl(RsrcInfo &&) const = 0; // nullable return value
      virtual void      rem_rsrc_impl(RsrcInfo &&)       = 0;
      
    public: /* Stored handle access */
      TaskHandle     task(const std::string &task_key);
      ResourceHandle resource(const std::string &task_key, const std::string &rsrc_key);
      ResourceHandle resource(const std::string &rsrc_key);

    public: /* Miscellaneous functions */
      // Rebuild underlying schedule from current list of submitted tasks/resources
      virtual void build() = 0;

      // Clear out tasks and owned resources; preserve_global t/f -> retain non-owned resources
      virtual void clear(bool preserve_global = true) = 0;

      // Debug output string formatted task schedule
      virtual std::list<std::string> schedule() const = 0;
      using RsrcMap = std::unordered_map<std::string, std::unordered_map<std::string, RsrcBasePtr>>;
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
      met_trace();
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
    
    // Stored handle access
    TaskHandle subtask(const std::string &task_key);
  };

  // Implementation base for resource handles, returned by scheduler/handle::task(...)/subtask(...)
  class TaskHandle {
    TaskInfo               m_task_key;
    detail::SchedulerBase *m_schd_handle;
    detail::TaskBase      *m_task_handle; // nullable

  public:
    TaskHandle(detail::SchedulerBase *schd_handle, TaskInfo key);
    
    // State queries
    bool is_init() const { return m_task_handle; } // on nullptr, return false
  
  public:
    /* Accessors */

    template <typename Ty>
    Ty & realize() { 
      static_assert(std::is_base_of_v<detail::TaskBase, Ty>);
      met_trace();
      debug::check_expr_rel(is_init(), "TaskHandle::realize<>() failed for empty task handle");
      return *static_cast<Ty *>(m_task_handle);
    }

    template <typename Ty, typename... Args>
    TaskHandle & init(Args... args) {
      static_assert(std::is_base_of_v<detail::TaskBase, Ty>);
      met_trace();
      m_task_handle = m_schd_handle->add_task_impl({ .prnt_key = m_task_key.prnt_key,
                                                     .task_key = m_task_key.task_key,
                                                     .ptr      = std::make_shared<Ty>(args...) });
      return *this;
    }

    template <typename Ty>
    TaskHandle & set(Ty &&task) {
      static_assert(std::is_base_of_v<detail::TaskBase, Ty>);
      met_trace();
      m_task_handle = m_schd_handle->add_task_impl({ .prnt_key = m_task_key.prnt_key,
                                                     .task_key = m_task_key.task_key,
                                                     .ptr      = std::make_shared<Ty>(std::move(task)) });
      return *this;
    }

    TaskHandle & dstr() {
      met_trace();
      m_schd_handle->rem_task_impl({ .prnt_key = m_task_key.prnt_key, .task_key = m_task_key.task_key });
      m_task_handle = nullptr;
      return *this;
    }
  };
  
  // Implementation base for resource handles, returned by scheduler/handle::resource(...)
  class ResourceHandle {
    RsrcInfo               m_rsrc_key;
    detail::SchedulerBase *m_schd_handle;
    detail::RsrcBase      *m_rsrc_handle; // nullable

  public:
    ResourceHandle(detail::SchedulerBase *schd_handle, RsrcInfo key);

    // State queries
    bool is_init()    const { return m_rsrc_handle;            } // on nullptr, return false
    bool is_mutated() const { return m_rsrc_handle->mutated(); }
    
  public: 
    /* Accessors */

    template <typename Ty> 
    const Ty & read_only() const { 
      met_trace();
      debug::check_expr_rel(is_init(), "ResourceHandle::read_only<>() failed for empty resource handle");
      return m_rsrc_handle->read_only<Ty>(); 
    }

    template <typename Ty>
    Ty & writeable() { 
      met_trace();
      debug::check_expr_rel(is_init(), "ResourceHandle::writeable<>() failed for empty resource handle");
      return m_rsrc_handle->writeable<Ty>(); 
    }

    /* Constr/destr */
  
    template <typename Ty, typename InfoTy = Ty::InfoType>
    ResourceHandle & init(InfoTy info) {
      met_trace();
      m_rsrc_handle = m_schd_handle->add_rsrc_impl({ .task_key = m_rsrc_key.task_key, 
                                                     .rsrc_key = m_rsrc_key.rsrc_key,
                                                     .ptr      = std::make_shared<detail::RsrcImpl<Ty>>(Ty(info)) });
      return *this;
    }

    template <typename Ty>
    ResourceHandle & set(Ty &&rsrc) {
      met_trace();
      m_rsrc_handle = m_schd_handle->add_rsrc_impl({ .task_key = m_rsrc_key.task_key, 
                                                     .rsrc_key = m_rsrc_key.rsrc_key,
                                                     .ptr      = std::make_shared<detail::RsrcImpl<Ty>>(std::move(rsrc)) });
      return *this;
    }

    ResourceHandle & dstr() {
      met_trace();
      m_schd_handle->rem_rsrc_impl({ .task_key = m_rsrc_key.task_key, .rsrc_key = m_rsrc_key.rsrc_key });
      m_rsrc_handle = nullptr;
      return *this;
    }
  };
} // namespace met