#pragma once

#include <initializer_list>
#include <tuple>
#include <span>
#include <utility>
#include <metameric/core/define.h>
#include <metameric/core/fwd.h>

namespace metameric {

struct GLObject {
  // Component accessors
  inline uint handle() const { return _handle; }
  inline bool is_init() const { return _is_init; }

protected:
  // Hidden constructor, virtual destructor for abstract object
  GLObject() = default; 
  virtual ~GLObject() = default;

  // Hidden constructor is used to initialize object
  explicit GLObject(bool init) noexcept 
  : _is_init(init) { }
  
  // Swap in-place with other object
  inline void swap(GLObject &o) {
    using std::swap;
    swap(_is_init, o._is_init);
    swap(_handle, o._handle);
  } 

  inline bool operator==(const GLObject &o) const {
    using std::tie;
    return tie(_is_init, _handle) 
        == tie(o._is_init, o._handle);
  }

  MET_DECLARE_NONCOPYABLE(GLObject)

protected:
  bool _is_init = false;
  uint _handle = 0;
};

enum class GLBufferTarget {
  eArray,
  eAtomicCounter,
  eCopyRead,
  eCopyWrite,
  eDrawIndirect,
  eDispatchIndirect,
  eElementArray,
  eShaderStorage,
  eQuery,
  eTexture,
  eTransformFeedback,
  eUniform,
};

enum class GLBufferStorageFlags : uint {
  eNone           = 0x0000,
  eDynamicStorage = 0x0100,
  eClientStorage  = 0x0200
};

MET_DECLARE_ENUM_FLAGS(GLBufferStorageFlags);

class GLBuffer : public GLObject {
  using Base = GLObject;

  size_t _size;
  uint _storage_flags;

public:
  // Size in bytes of buffer storage
  inline size_t size() const { return _size; }

  // Underlying storage flags matching GLBufferStorageFlags
  inline uint storage_flags() const { return _storage_flags; }

  // Test presence of specific storage flag matching GLBufferStorageFlags
  inline bool has_storage_flag(GLBufferStorageFlags f) { return (_storage_flags & (uint) f) != 0u; }

public:
  // Base constructors to setup and tear down buffer storage
  GLBuffer() = default;
  GLBuffer(size_t size, void const *data = nullptr, uint storage_flags = 0u);
  ~GLBuffer();

  // Convenience constructors accepting a number of STL types
  template <typename T>
  GLBuffer(std::initializer_list<const T> c, uint storage_flags = 0u)
  : GLBuffer(c.size() * sizeof(T), std::data(c), storage_flags) { }
  template <typename T, size_t E>
  GLBuffer(std::span<const T, E> c, uint storage_flags = 0u) 
  : GLBuffer(c.size() * sizeof(T), std::data(c), storage_flags) { }
  template <typename C>
  GLBuffer(const C &c, uint storage_flags = 0u)
  : GLBuffer(c.size() * sizeof(typename C::value_type), std::data(c), storage_flags) { }

public:
  // Base get/set/fill/clear functions
  void get(void *data, size_t size = 0, size_t offset = 0) const;
  void set(void const *data, size_t size = 0, size_t offset = 0);
  void fill(void const *data, size_t stride = 1, size_t size = 0, size_t offset = 0);
  void clear(size_t size = 0, size_t offset = 0);

  // Convenience get overrides accepting a number of STL types
  template <typename T, size_t E>
  auto get(std::span<T, E> c) const    { get(std::data(c), c.size() * sizeof(T)); return c; }
  template <typename C>
  auto & get(C &c) const               { get(std::data(c), c.size() * sizeof(typename C::value_type)); return c; }

  // Convenience set overrides accepting a number of STL types
  template <typename T>
  void set(std::initializer_list<T> c) { set(std::data(c), c.size() * sizeof(T)); }
  template <typename T, size_t E>
  void set(std::span<T, E> c)          { set(std::data(c), c.size() * sizeof(T)); }
  template <typename C>
  void set(const C &c)                 { set(std::data(c), c.size() * sizeof(typename C::value_type)); }

  // Convenience fill overrides accepting a number of STL types
  template <typename T>
  void fill(std::initializer_list<T> c) { fill(std::data(c), c.size()); }
  template <typename T, size_t E>
  void fill(std::span<T, E> c)          { fill(std::data(c), c.size()); }
  template <typename C>
  void fill(const C &c)                 { fill(std::data(c), c.size()); }

  // Convenience get_as which constructs and writes directly into vector/list container
  template <typename C>
  C get_as() const { C c(size() / sizeof(typename C::value_type)); return get(c); }

  // Copy constr/assign is deleted to prevent accidental usage, but an explicit copy can still 
  // be performed between (parts of) buffers, if truly unavoidable
  GLBuffer copy(size_t size = 0, size_t offset = 0) const;
  void copy_from(const GLBuffer &other, size_t size = 0, size_t r_offset = 0, size_t w_offset = 0);
  void copy_to(GLBuffer &other, size_t size = 0, size_t r_offset = 0, size_t w_offset = 0) const;

public:
  // Convenience merge of bind/range bind
  void bind(GLBufferTarget target, uint index, size_t offset = 0, size_t size = 0) const;

public:
  inline
  void swap(GLBuffer &o) {
    using std::swap;
    Base::swap(o);
    swap(_size, o._size);
    swap(_storage_flags, o._storage_flags);
  }
  
  inline
  bool operator==(const GLBuffer &o) const {
    using std::tie;
    return Base::operator==(o)
      && tie(_size, _storage_flags)
      == tie(o._size, o._storage_flags);
  }

  // Define move constr/assign, but do not allow direct copies to prevent accidental usage
  MET_DECLARE_NONCOPYABLE(GLBuffer)
};

} // namespace metameric