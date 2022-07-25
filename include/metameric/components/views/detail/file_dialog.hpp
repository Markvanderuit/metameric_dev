#pragma once

#include <metameric/core/io.hpp>
#include <metameric/core/math.hpp>

namespace met::detail {
  bool load_dialog(fs::path &path, const std::string &type_filter = "");
  bool save_dialog(fs::path &path, const std::string &type_filter = "");
} // namespace met::detail