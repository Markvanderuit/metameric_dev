// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <metameric/core/detail/scheduler_base.hpp>

namespace met {
  // Implementation base for resource handles, returned by scheduler/handle::task(...)/child_task(...)
  class TaskHandle {
    detail::TaskInfo       m_task_key;
    detail::SchedulerBase *m_schd_handle = nullptr;
    detail::TaskNode      *m_task_handle = nullptr;

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
    detail::SchedulerBase *m_schd_handle = nullptr;
    detail::RsrcNode      *m_rsrc_handle = nullptr;

  public:
    ResourceHandle() = default;
    ResourceHandle(detail::SchedulerBase *schd_handle, detail::RsrcInfo key);

    // State queries
    bool is_init()    const { return m_rsrc_handle;            } // on nullptr, return false
    bool is_mutated() const { return m_rsrc_handle->mutated(); }

    // Info queries
    const std::string & task_key() const { return m_rsrc_key.task_key; }
    const std::string & rsrc_key() const { return m_rsrc_key.rsrc_key; }
    
    // Reinitialize w.r.t. active scheduler
    void reinitialize(SchedulerHandle &info) {
      *this = ResourceHandle(&info, m_rsrc_key);
    }

  public: /* Accessors */
    template <typename Ty> 
    const Ty & getr() const { 
      met_trace();
      debug::check_expr(is_init(), "ResourceHandle::getr<>() failed for empty resource handle");
      return m_rsrc_handle->getr<Ty>(); 
    }

    template <typename Ty>
    Ty & getw() { 
      met_trace();
      debug::check_expr(is_init(), "ResourceHandle::getw<>() failed for empty resource handle");
      return m_rsrc_handle->getw<Ty>(); 
    }
  
    template <typename Ty, typename InfoTy = Ty::InfoType>
    ResourceHandle & init(const InfoTy &info) {
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