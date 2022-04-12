#pragma once

#include <metameric/core/define.h>
#include <metameric/core/exception.h>
#include <metameric/core/vlk/types.h>

namespace metameric::vlk::detail {
  inline
  void assert_impl(vk::Result result, const std::string &msg, const char *file_path, uint line_nr) {
    guard(result != vk::Result::eSuccess);

    metameric::detail::RuntimeException e(msg);
    e["file_path"] = file_path;
    e["line_nr"] = std::to_string(line_nr);
    e["vk_res"] = vk::to_string(result);
    
    throw e;
  }
} // namespace metameric::vlk::detail


#define vlk_assert(result, msg)\
  metameric::vlk::detail::assert_impl(result, msg, __FILE__, __LINE__);