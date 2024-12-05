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

#include <metameric/core/scheduler.hpp>

namespace met {
  // Submit editor tasks to program schedule, s.t. a default empty view is given
  void submit_editor_schedule_unloaded(detail::SchedulerBase &scheduler);

  // Submit editor tasks to program schedule, s.t. a scene editor is shown for the current scene
  void submit_editor_schedule_loaded(detail::SchedulerBase &scheduler);

  // Submit editor tasks to program schedule, dependent on current scene state in scheduler data
  void submit_editor_schedule_auto(detail::SchedulerBase &scheduler);
} // namespace met