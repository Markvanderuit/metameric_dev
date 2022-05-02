#pragma once

#include <metameric/core/exception.h>
#include <glad/glad.h>

namespace metameric::gl {
  inline
  void err_assert(const std::string &msg = "",
                  const std::source_location loc = std::source_location::current()) {
    GLenum err = glGetError();
    guard(err != GL_NO_ERROR);

    metameric::detail::RuntimeException e(msg);
    e["src"]   = "metameric::gl::err_assert";
    e["file"]   = fmt::format("{}({}:{})", loc.file_name(), loc.line(), loc.column());
    e["func"]  = loc.function_name();
    e["gl_err"] = std::to_string(err);
    
    throw e;
  }
} // namespace metameric::gl