#pragma once

#include <metameric/core/detail/scheduler_base.hpp>

namespace met {
  // Implementation base for resource handles, returned by scheduler/handle::task(...)/child_task(...)
  class TaskHandle {
    detail::TaskInfo       m_task_key;
    detail::SchedulerBase *m_schd_handle;
    detail::TaskNode      *m_task_handle; // nullable

  public:
    TaskHandle(detail::SchedulerBase *schd_handle, detail::TaskInfo key);
    
    // State queries
    bool is_init() const { return m_task_handle; } // on nullptr, return false

    // Info queries
    std::string key() const { 
      return m_task_key.prnt_key.empty() 
        ? m_task_key.task_key
        : fmt::format("{}.{}", m_task_key.prnt_key, m_task_key.task_key);
    } 

    MaskedSchedulerHandle mask(SchedulerHandle &handle) const;

  public: /* Accessors */
    template <typename Ty>
    Ty & realize() { 
      static_assert(std::is_base_of_v<detail::TaskNode, Ty>);
      met_trace();
      debug::check_expr(is_init(), "TaskHandle::realize<>() failed for empty task handle");
      return *static_cast<Ty *>(m_task_handle);
    }

    template <typename Ty, typename... Args>
    TaskHandle & init(Args... args) {
      static_assert(std::is_base_of_v<detail::TaskNode, Ty>);
      met_trace();
      m_task_handle = m_schd_handle->add_task_impl({ .prnt_key = m_task_key.prnt_key,
                                                     .task_key = m_task_key.task_key,
                                                     .ptr      = std::make_shared<Ty>(args...) });
      return *this;
    }

    template <typename Ty>
    TaskHandle & set(Ty &&task) {
      static_assert(std::is_base_of_v<detail::TaskNode, Ty>);
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
    detail::RsrcInfo       m_rsrc_key;
    detail::SchedulerBase *m_schd_handle;
    detail::RsrcNode      *m_rsrc_handle; // nullable

  public:
    ResourceHandle(detail::SchedulerBase *schd_handle, detail::RsrcInfo key);

    // State queries
    bool is_init()    const { return m_rsrc_handle;            } // on nullptr, return false
    bool is_mutated() const { return m_rsrc_handle->mutated(); }

    // Info queries
    const std::string & task_key() const { return m_rsrc_key.task_key; }
    const std::string & rsrc_key() const { return m_rsrc_key.rsrc_key; }
    
  public: /* Accessors */
    template <typename Ty> 
    const Ty & read_only() const { 
      met_trace();
      debug::check_expr(is_init(), "ResourceHandle::read_only<>() failed for empty resource handle");
      return m_rsrc_handle->read_only<Ty>(); 
    }

    template <typename Ty>
    Ty & writeable() { 
      met_trace();
      debug::check_expr(is_init(), "ResourceHandle::writeable<>() failed for empty resource handle");
      return m_rsrc_handle->writeable<Ty>(); 
    }
  
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