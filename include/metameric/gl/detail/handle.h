#pragma once

#include <utility>

namespace metameric::gl {
  template <typename T = uint>
  class Handle {
  public:
    T object() const { return _object; }
    T& object() { return _object; }
    bool is_init() const { return _is_init; }

    // Returns an uninitialized framebuffer to act as default framebuffer.
    static Handle empty_handle() { return Handle(); }

  protected:
    bool _is_init = false;
    T _object = 0;

    constexpr Handle() = default;
    constexpr Handle(bool init) noexcept : _is_init(init) { }
    constexpr virtual ~Handle() = default;

    inline constexpr void swap(Handle &o) {
      using std::swap;
      swap(_is_init, o._is_init);
      swap(_object, o._object);
    }

    inline constexpr bool operator==(const Handle &o) const {
      using std::tie;
      return tie(_is_init, _object) 
          == tie(o._is_init, o._object);
    }

    MET_NONCOPYABLE_CONSTR(Handle)
  };
} // namespace metameric::gl