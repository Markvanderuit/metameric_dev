#pragma once

#include <metameric/gl/detail/fwd.h>
#include <metameric/gl/detail/handle.h>
#include <metameric/gl/enums.h>

namespace metameric::gl {
  class Buffer : Handle<> {
  public:
    Buffer() = default;
    Buffer(size_t size, 
           const void *data,
           BufferStorageFlags flags = {});
    ~Buffer();

    inline constexpr
    void swap(Buffer &o) {
      using std::swap;
      Handle::swap(o);
      // ...
    }

    inline constexpr
    bool operator==(const Buffer &o) const {
      using std::tie;
      return Handle::operator==(o);
          // && tie(_is_init, _value) 
          // == tie(o._is_init, o._value);
    }

    MET_NONCOPYABLE_CONSTR(Buffer);
  };
} /* namespace metameric::gl */