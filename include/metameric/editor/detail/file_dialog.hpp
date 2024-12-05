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

#include <metameric/core/io.hpp>
#include <metameric/core/math.hpp>

namespace met::detail {
  // Spawn a file load dialog; type filters is a list of preferred types,
  // e.g. { "exr", "png", "jpg" }
  bool load_dialog(fs::path &path, 
                   std::initializer_list<std::string> type_filters = {});
                   
  // Spawn a file save dialog; type filters is a list of preferred types,
  // e.g. { "exr", "png", "jpg" }
  bool save_dialog(fs::path &path, 
                   std::initializer_list<std::string> type_filters = {});
} // namespace met::detail