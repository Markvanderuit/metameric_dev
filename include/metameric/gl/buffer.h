#pragma once

#include <metameric/gl/detail/fwd.h>
#include <metameric/gl/detail/handle.h>
#include <span>

namespace metameric::gl {
  /**
   * Helper data object to construct a buffer with default settings. Works 
   * with aggregate/designated initialization.
   */
  struct BufferCreateInfo {
    size_t size;
    std::span<std::byte> data = { };
    BufferStorageFlags flags = { };
  };

  /**
   * Buffer object.
   */
  class Buffer : public Handle<> {
    using Base = Handle<>;

    bool _is_mapped;
    size_t _size;

  public:
    /* constr/destr */

    Buffer() = default;
    Buffer(BufferCreateInfo info);
    ~Buffer();

    /* getters/setters */

    inline size_t size() const { return _size; }
    inline bool is_mapped() const { return _is_mapped; }

    /* operands */

    void get(std::span<std::byte> data, 
             size_t size = 0,
             size_t offset = 0) const;

    void set(std::span<std::byte> data,
             size_t size = 0,
             size_t offset = 0);

    void clear(std::span<std::byte> data = { },
               size_t stride = 1,
               size_t size = 0,
               size_t offset = 0);

    /* state */

    void bind_to(BufferTarget target, 
                 uint index,
                 size_t size = 0,
                 size_t offset = 0) const;

    /* mapping */

    std::span<std::byte> map(size_t size = 0, 
                             size_t offset = 0,
                             BufferAccessFlags flags = { });
    void flush(size_t size = 0, 
               size_t offset = 0);
    void unmap();

    /* miscellaneous */

    inline constexpr void swap(Buffer &o) {
      using std::swap;
      Base::swap(o);
      swap(_size, o._size);
      swap(_is_mapped, o._is_mapped);
    }

    inline constexpr bool operator==(const Buffer &o) const {
      using std::tie;
      return Base::operator==(o)
        && tie(_size, o._is_mapped)
        == tie(o._size, o._is_mapped);
    }

    MET_NONCOPYABLE_CONSTR(Buffer);
  };
} /* namespace metameric::gl */