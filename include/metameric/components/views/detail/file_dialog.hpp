#pragma once

#include <metameric/core/io.hpp>
#include <metameric/core/math.hpp>

namespace met::detail {
  // Spawn a file load dialog; type filters is a list of preferred types,
  // e.g. { "exr", "png", "jpg" }
  bool load_dialog(fs::path &path, 
                   std::initializer_list<std::string> type_filters = {});
                   
  // Spawn a file load dialog allowing multiple selects; type filters 
  // is a list of preferred types, e.g. { "exr", "png", "jpg" }
  bool load_dialog_mult(std::vector<fs::path> &paths, 
                        std::initializer_list<std::string> type_filters = {});

                        
  // Spawn a file save dialog; type filters is a list of preferred types,
  // e.g. { "exr", "png", "jpg" }
  bool save_dialog(fs::path &path, 
                   std::initializer_list<std::string> type_filters = {});
} // namespace met::detail