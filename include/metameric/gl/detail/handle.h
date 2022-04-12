#pragma once

#include <utility>
#include <metameric/gl/detail/fwd.h>

namespace metameric::gl {
  template <typename T>
  struct AbstractAllocator {
    static inline T alloc();
    static inline void destroy(T t);
  };

  template <typename T, typename Alloc>
  class AbstractHandle {
  public:
    inline T object() const { return _object; }

  protected:
    AbstractHandle() : _object(Alloc::alloc()) { }
    ~AbstractHandle() { Alloc::destroy(_object); }

    inline constexpr void swap(AbstractHandle &o) {  std::swap(_object, o._object); }
    inline constexpr bool operator==(const AbstractHandle &o) const { return _object == o._object; }

    T _object;
  };

  template <typename T = uint>
  class Handle {
  public:
    T object() const { return _object; }
    T& object() { return _object; }
    bool is_init() const { return _is_init; }

  protected:
    bool _is_init;
    T _object;

    constexpr explicit Handle(bool init = false) noexcept : _is_init(init) { }
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