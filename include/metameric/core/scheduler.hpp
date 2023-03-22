#pragma once

#include <metameric/core/detail/scheduler_base.hpp>
#include <metameric/core/math.hpp>
#include <memory>
#include <span>
#include <sstream>
#include <string>

namespace met {
  class LinearScheduler : public SchedulerBase {
    using RsrcMap  = std::unordered_map<std::string, std::unordered_map<std::string, detail::RsrcNode>>;
    using TaskMap  = std::unordered_map<std::string, detail::TaskNode>;
    
  public:
    
  private:
    // Private members
    RsrcMap                  m_rsrc_registry;
    TaskMap                  m_task_registry;
    std::vector<std::string> m_task_order;

    // Virtual method implementations
    virtual void              add_task_impl(TaskInfo &&)       override;
    virtual detail::TaskBase *get_task_impl(TaskInfo &&) const override; // nullable return value
    virtual void              rem_task_impl(TaskInfo &&)       override;
    virtual detail::RsrcBase *add_rsrc_impl(RsrcInfo &&)       override; // nullable return value
    virtual detail::RsrcBase *get_rsrc_impl(RsrcInfo &&) const override; // nullable return value
    virtual void              rem_rsrc_impl(RsrcInfo &&)       override;

    // Virtual method implementations
    virtual void run_clear_state_impl() override;
    virtual void run_schedule_impl()    override;

  public: /* public methods */
    // Friend class has access to internal components
    friend class LinearSchedulerHandle;
    
    virtual void build() override;                            // Build schedule given current tasks/resources
    virtual void clear(bool preserve_global = true) override; // Clear current schedule and resources

    // Debug methods
    virtual std::vector<std::string> schedule() const override { return m_task_order; }
    virtual const RsrcMap &resources() const override { return m_rsrc_registry; }
  };

  /**
   * Class that consumes application tasks and updates the environment
   * in which they exist.
   */
  class LinearSchedulerHandle : public SchedulerHandle {
  public:
    // Signal flags passed back by handle object back to scheduler
    enum class HandleReturnFlags : uint {
      eNone       = 0x000u, // Default value
      eClearTasks = 0x001u, // Signal that tasks and owned resources are to be destroyed after run
      eClearAll   = 0x002u, // Signal that tasks and global resources are to be destroyed after run
      eBuild      = 0x004u, // Signal that schedule needs to be rebuilt after run
    };
    
  protected:
    // Private members
    LinearScheduler &m_scheduler;
    std::string      m_task_key;

    // Friend private members
    HandleReturnFlags   return_flags;
    std::list<TaskInfo> add_task_info;
    std::list<TaskInfo> rem_task_info;

    // Virtual method implementations
    virtual void              add_task_impl(TaskInfo &&)       override;
    virtual detail::TaskBase *get_task_impl(TaskInfo &&) const override; // nullable return value
    virtual void              rem_task_impl(TaskInfo &&)       override;
    virtual detail::RsrcBase *add_rsrc_impl(RsrcInfo &&)       override; // nullable return value
    virtual detail::RsrcBase *get_rsrc_impl(RsrcInfo &&) const override; // nullable return value
    virtual void              rem_rsrc_impl(RsrcInfo &&)       override;

  public:
    // Friend class has access to internal components
    friend class LinearScheduler;

    LinearSchedulerHandle(LinearScheduler &scheduler, const std::string &task_key)
    : m_scheduler(scheduler),
      m_task_key(task_key),
      return_flags(HandleReturnFlags::eNone) { }

    virtual void build() override;                            // Build schedule given current tasks/resources
    virtual void clear(bool preserve_global = true) override; // Clear current schedule and resources

    // Get key of current active task
    virtual const std::string &task_key() const override { return m_task_key; };

    // Debug methods
    virtual std::vector<std::string> schedule() const override { return m_scheduler.m_task_order; }
    virtual const RsrcMap &resources() const override { return m_scheduler.resources(); }
  };

  class MaskedSchedulerHandle : public SchedulerHandle {
  protected:
    SchedulerHandle &m_masked_handle;
    std::string      m_task_key;

    // Virtual method implementations
    virtual void              add_task_impl(TaskInfo &&)       override;
    virtual detail::TaskBase *get_task_impl(TaskInfo &&) const override; // nullable return value
    virtual void              rem_task_impl(TaskInfo &&)       override;
    virtual detail::RsrcBase *add_rsrc_impl(RsrcInfo &&)       override; // nullable return value
    virtual detail::RsrcBase *get_rsrc_impl(RsrcInfo &&) const override; // nullable return value
    virtual void              rem_rsrc_impl(RsrcInfo &&)       override;

  public:
    MaskedSchedulerHandle(SchedulerHandle &masked_handle, const std::string &task_key, bool is_full_key = false)
    : m_masked_handle(masked_handle),
      m_task_key(is_full_key ? task_key : fmt::format("{}.{}", masked_handle.task_key(), task_key)) { }

    virtual void build()                            override; // Build schedule given current tasks/resources
    virtual void clear(bool preserve_global = true) override; // Clear current schedule and resources

    // Get key of current masking task
    virtual const std::string &task_key() const override { return m_task_key; };

    // Debug methods
    virtual std::vector<std::string> schedule() const override { return m_masked_handle.schedule(); }
    virtual const RsrcMap &resources() const override { return m_masked_handle.resources(); }
  };

  met_declare_bitflag(LinearSchedulerHandle::HandleReturnFlags);
} // namespace met