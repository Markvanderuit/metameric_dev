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

#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/editor/detail/file_dialog.hpp>
#include <vector>
#include <tinyfiledialogs.h>

namespace met::detail {
  bool load_dialog(fs::path &path, std::initializer_list<std::string> type_filters) {
    met_trace();

    // Build list of filter patterns
    auto filter_ptr = type_filters
                    | vws::transform([](const std::string &name) { return name.c_str(); })
                    | view_to<std::vector<const char *>>();

    auto c_str = tinyfd_openFileDialog("Load file", nullptr, filter_ptr.size(), filter_ptr.data(), nullptr, 0); 
    if (c_str) {
      path = std::string(c_str);
      return true;
    } else {
      return false;
    }
  }

  bool save_dialog(fs::path &path, std::initializer_list<std::string> type_filters) {
    met_trace();

    // Build list of filter patterns
    auto filter_ptr = type_filters
                    | vws::transform([](const std::string &name) { return name.c_str(); })
                    | view_to<std::vector<const char *>>();
    
    auto c_str = tinyfd_saveFileDialog("Save file", nullptr, filter_ptr.size(), filter_ptr.data(), nullptr);
    if (c_str) {
      path = std::string(c_str);
      return true;
    } else {
      return false;
    }
  }
} // namespace met::detail