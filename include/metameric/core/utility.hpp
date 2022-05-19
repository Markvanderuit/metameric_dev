#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/detail/utility.hpp>
#include <fmt/format.h>
#include <glm/glm.hpp>
#include <filesystem>
#include <source_location>

// Simple guard statement syntactic sugar
#define guard(expr,...) if (!(expr)) { return __VA_ARGS__ ; }
#define guard_continue(expr) if (!(expr)) { continue; }
#define guard_break(expr) if (!(expr)) { break; }

namespace met {
  namespace io {
    // Return object for load_texture_*(...) below
    template <typename Ty>
    struct TextureData {
      std::vector<Ty> data;
      glm::ivec2 size;
      int channels;
    };

    // Load raw texture data from the given filepath
    TextureData<std::byte> load_texture_byte(std::filesystem::path path);
    
    // Load float-scaled texture data from the given filepath
    TextureData<float> load_texture_float(std::filesystem::path path);

    // Linearize/delinearize srgb texture data
    void apply_srgb_to_lrgb(TextureData<float> &obj, bool skip_alpha = true);
    void apply_lrgb_to_srgb(TextureData<float> &obj, bool skip_alpha = true);
  };

  namespace debug {
    // Evaluate a boolean expression, throwing a detailed exception pointing
    // to the expression's origin if said expression fails
    inline
    void check_expr(bool expr,
                    const std::string_view &msg = "",
                    const std::source_location sl = std::source_location::current()) {
  #ifdef NDEBUG
  #else
      guard(!expr);

      detail::Exception e;
      e.put("src", "met::debug::check_expr(...) failed, checked expression evaluated to false");
      e.put("message", msg);
      e.put("in file", fmt::format("{}({}:{})", sl.file_name(), sl.line(), sl.column()));
      throw e;
  #endif
    }
  } // namespace debug
} // namespace met