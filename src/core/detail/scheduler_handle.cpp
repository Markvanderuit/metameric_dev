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

#include <metameric/core/detail/scheduler_handle.hpp>

namespace met {
  ResourceHandle::ResourceHandle(detail::SchedulerBase *schd_handle, detail::RsrcInfo key)
  : m_rsrc_key(key), 
    m_schd_handle(schd_handle), 
    m_rsrc_handle(m_schd_handle->get_rsrc_impl(std::move(key))) { }

  TaskHandle::TaskHandle(detail::SchedulerBase *schd_handle, detail::TaskInfo key)
  : m_task_key(key), 
    m_schd_handle(schd_handle), 
    m_task_handle(m_schd_handle->get_task_impl(std::move(key))) { }

  MaskedSchedulerHandle TaskHandle::mask(SchedulerHandle &handle) const {
    return MaskedSchedulerHandle(handle, key());
  }
} // namespace met