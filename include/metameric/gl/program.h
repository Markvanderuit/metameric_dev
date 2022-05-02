#pragma once

#include <metameric/gl/detail/fwd.h>
#include <metameric/gl/detail/handle.h>
#include <metameric/gl/utility.h>
#include <initializer_list>
#include <string_view>
#include <span>
#include <unordered_map>

namespace metameric::gl {
  struct ShaderLoadInfo {
    ShaderType type;
    fs::path file_path;
    bool is_binary_spirv = true;
    std::string entry_point = "main";
  };

  struct ShaderCreateInfo {
    ShaderType type;
    std::span<const std::byte> data;
    bool is_binary_spirv = true;
    std::string entry_point = "main";
  };

  class Program : public Handle<> {
    using Base = Handle<>;

    std::unordered_map<std::string, int> _loc;
    int loc(std::string_view s);

  public:
    /* constr/destr */

    Program() = default;
    Program(std::initializer_list<ShaderLoadInfo> info);
    Program(std::initializer_list<ShaderCreateInfo> info);
    ~Program();

    /* state management */  

    template <typename T>
    void uniform(std::string_view s, T t);
    void bind() const;
    void unbind() const;

    /* miscellaneous */  

    inline void swap(Program &o) {
      using std::swap;
      Base::swap(o);
      swap(_loc, o._loc);
    }

    inline bool operator==(const Program &o) const {
      return Base::operator==(o) && _loc == o._loc;
    }

    MET_NONCOPYABLE_CONSTR(Program);
  };
} // namespace metameric::gl