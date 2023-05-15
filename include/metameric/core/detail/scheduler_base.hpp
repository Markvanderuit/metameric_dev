#pragma once

#include <metameric/core/utility.hpp>
#include <memory>
#include <string>

namespace met {
  // Forward declarations
  class Scheduler;
  class ResourceHandle;
  class TaskHandle;
  class SchedulerHandle;
  class MaskedSchedulerHandle;

  namespace detail {
    // Forward declarations
    class SchedulerBase;
    struct TaskNode;
    struct RsrcNode;

    // Shorthands for shared_ptr wrapper
    using TaskBasePtr = std::shared_ptr<TaskNode>;
    using RsrcBasePtr = std::shared_ptr<RsrcNode>;

    // Internal parameter objects for adding/removing/accessing tasks/resource nodes in a schedule
    struct TaskInfo { std::string prnt_key = "", task_key = ""; detail::TaskBasePtr ptr = nullptr; };
    struct RsrcInfo { std::string task_key = "", rsrc_key = ""; detail::RsrcBasePtr ptr = nullptr; };

    // Abstract base class for tasks submitted to application scheduler
    // Implementations contain majority of program code
    struct TaskNode {
      // Override and implement; setup of task
      virtual void init(SchedulerHandle &) { };

      // Override and implement; main body of task
      virtual void eval(SchedulerHandle &) { };

      // Override and implement; teardown of task
      virtual void dstr(SchedulerHandle &) { };

      // Override and implement; on false return, eval(...) is not called
      virtual bool is_active(SchedulerHandle &) { return true; }
    };

    // Abstract base class for application resources;
    template <typename> struct RsrcImpl; // FWD of container class
    class RsrcNode {
    private:
      bool m_is_mutated = true;

    public:
      // State queries; the resource is either modified, or not
      void set_mutated(bool b) { m_is_mutated = b; }
      bool mutated() const { return m_is_mutated; }

      template <typename Ty>
      const Ty & read_only() const {
        met_trace();
        return static_cast<const RsrcImpl<Ty> *>(this)->m_object;
      }

      template <typename Ty>
      Ty & writeable() {
        met_trace();
        set_mutated(true);
        return static_cast<RsrcImpl<Ty> *>(this)->m_object;
      }
    };

    // Implementation class for application resource to hold specific type
    template <typename T>
    struct RsrcImpl : public RsrcNode {
      T m_object;

      RsrcImpl(T &&object) : m_object(std::move(object)) { }
    };
  } // namespace detail

  namespace detail {
    class SchedulerBase {
    public: /* Virtual implementation functions for add/remove/get of tasks/resources */
      virtual TaskNode *add_task_impl(detail::TaskInfo &&)       = 0; // nullable return value
      virtual TaskNode *get_task_impl(detail::TaskInfo &&) const = 0; // nullable return value
      virtual void      rem_task_impl(detail::TaskInfo &&)       = 0;
      virtual RsrcNode *add_rsrc_impl(detail::RsrcInfo &&)       = 0; // nullable return value
      virtual RsrcNode *get_rsrc_impl(detail::RsrcInfo &&) const = 0; // nullable return value
      virtual void      rem_rsrc_impl(detail::RsrcInfo &&)       = 0;
      
    public: /* Miscellaneous functions */
      // Task node handle access
      TaskHandle task(const std::string &task_key);

      // Resource node handle access
      ResourceHandle global(const std::string &rsrc_key);                                  // Handle to unowned resource
      ResourceHandle resource(const std::string &task_key, const std::string &rsrc_key);   // Handle to a task's resource
      ResourceHandle operator()(const std::string &task_key, const std::string &rsrc_key); // Handle to a task's resource

      // Clear out tasks and owned resources; preserve_global t/f -> retain non-owned resources
      virtual void clear(bool preserve_global = true) = 0;
    };
  } // namespace detail

  // Virtual base class for application sscheduler
  struct Scheduler : public detail::SchedulerBase {
    using detail::SchedulerBase::task;
    using detail::SchedulerBase::global;
    using detail::SchedulerBase::resource;
    using detail::SchedulerBase::operator();
    
    // Run the currently built schedule
    virtual void run() = 0;
  };

  // Virtual base class for scheduler handle, passed to task nodes
  class SchedulerHandle : public detail::SchedulerBase {
    std::string m_task_key;

  public:
    using detail::SchedulerBase::task;
    using detail::SchedulerBase::global;
    using detail::SchedulerBase::resource;
    using detail::SchedulerBase::operator();

    SchedulerHandle(const std::string &task_key)
    : m_task_key(task_key) { }
    
    // Task node handle access
    TaskHandle task();                                      // Handle to current task
    TaskHandle parent_task();                               // Handle to parent task, relative to current task
    TaskHandle child_task(const std::string &task_key);     // Handle to child task relative to current task
    TaskHandle relative_task(const std::string &task_key);  // Handle to relative task, on same level as current task

    // Resource node handle access
    ResourceHandle resource(const std::string &rsrc_key);   // Handle to current task's resource
    ResourceHandle operator()(const std::string &rsrc_key); // Handle to current task's resource

    // Masked scheduler handle shorthands, to navigate task structure
    MaskedSchedulerHandle parent();                              // Masked scheduler handle for parent task
    MaskedSchedulerHandle child(const std::string &task_key);    // Masked scheduler handle for child task
    MaskedSchedulerHandle relative(const std::string &task_key); // Masked scheduler handle for relative task
  };
  
  // Implementing class for masked scheduler handle, passed to containing task nodes
  class MaskedSchedulerHandle : public SchedulerHandle {
    SchedulerHandle &m_handle;
    
    // Virtual method implementations for SchedulerHandle
    detail::TaskNode *add_task_impl(detail::TaskInfo &&info)       override { return m_handle.add_task_impl(std::move(info)); }
    detail::TaskNode *get_task_impl(detail::TaskInfo &&info) const override { return m_handle.get_task_impl(std::move(info)); }
    void              rem_task_impl(detail::TaskInfo &&info)       override {        m_handle.rem_task_impl(std::move(info)); }
    detail::RsrcNode *add_rsrc_impl(detail::RsrcInfo &&info)       override { return m_handle.add_rsrc_impl(std::move(info)); }
    detail::RsrcNode *get_rsrc_impl(detail::RsrcInfo &&info) const override { return m_handle.get_rsrc_impl(std::move(info)); }
    void              rem_rsrc_impl(detail::RsrcInfo &&info)       override {        m_handle.rem_rsrc_impl(std::move(info)); }

  public:
    MaskedSchedulerHandle(SchedulerHandle &handle, const std::string &task_key)
    : SchedulerHandle(task_key),
      m_handle(handle) { }

    void clear(bool preserve_global = true) override { m_handle.clear(preserve_global); }
  };
} // namespace met