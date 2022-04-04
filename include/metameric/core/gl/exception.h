#pragma once

#include <metameric/core/exception.h>
#include <glad/glad.h>

namespace metameric {
  namespace detail {
    inline
    void gl_assert_impl(const std::string &msg, const char *file_path, uint line_nr) {
      GLenum err = glGetError();
      guard(err != GL_NO_ERROR);

      RuntimeException e(msg);
      e["file_path"] = file_path;
      e["line_nr"] = std::to_string(line_nr);
      e["gl_err"] = std::to_string(err);

      throw e;
    }
  } // namespace detail

  #define gl_assert(msg) detail::gl_assert_impl(msg, __FILE__, __LINE__);
} // namespace metameric