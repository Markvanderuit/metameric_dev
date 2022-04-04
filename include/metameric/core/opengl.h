#pragma once

#include <utility>
#include <initializer_list>
#include <tuple>
#include <vector>
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

  MET_CLASS_NON_COPYABLE_CONSTR(GLObject)

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

class GLBuffer : public GLObject {
  using Base = GLObject;

public:
  inline size_t size() const { return _size; }

  /*
    - set subset of data to multiple values
    - set all data to multiple values
    - set subset of data to one value
    - set all data to one value

    - get subset of data
    - get all data

    - copy subset of data to other buffer
    - copy all data to other buffer
  */

  template <typename T>
  void set_sub_data(size_t size, const T &t) { set_sub_data(size, t.data()); }
  template <typename T>
  void set_sub_data(size_t size, size_t offset, const T &t) { set_sub_data(size, offset, t.data()); }

  void set_sub_data(size_t size, void const *data);
  void set_sub_data(size_t size, size_t offset, void const *data);

  template <typename T>
  void get_sub_data(size_t size, T &t) { get_sub_data(size, t.data()); }
  template <typename T>
  void get_sub_data(size_t size, size_t offset, T &t) { get_sub_data(size, offset, t.data()); }

  void get_sub_data(size_t size, void *data);
  void get_sub_data(size_t size, size_t offset, void *data);

  template <typename T>
  void clear_data(const T &t) { clear_data(t.data()); }
  template <typename T>
  void clear_sub_data(size_t size, size_t offset, const T &t) { clear_sub_data(size, offset, t.data()); }

  void clear_data(void const *data);
  void clear_sub_data(size_t size, size_t offset, void const *data);
  
  void copy_data(size_t size,
                 size_t first_offset, 
                 size_t second_offset,
                 const GLBuffer &o);

  void bind_to(GLBufferTarget target);

public:
  // Default constructor and overloaded destructor to free resources
  GLBuffer() = default;
  ~GLBuffer();

  // Explicit constructor with optional host pointer
  explicit GLBuffer(size_t size, void const *data = nullptr);

  // Convenience initializer list overload
  template <typename T>
  GLBuffer(std::initializer_list<T> l) 
  : GLBuffer(l.size() * sizeof(T), l.begin()) {}
  
  // Convenience vector container overload
  template <typename T>
  GLBuffer(const std::vector<T>& v)
  : GLBuffer(v.size() * sizeof(T), v.data()) {}
  
  // Swap in-place with other object
  inline void swap(GLBuffer &o) {
    Base::swap(o);
    using std::swap;
    swap(_size, o._size);
  }

  inline bool operator==(const GLBuffer &o) const {
    using std::tie;
    return Base::operator==(o)
        && tie(_size)
        == tie(o._size);
  }

  MET_CLASS_NON_COPYABLE_CONSTR(GLBuffer)

private:
  size_t _size;
};

} // namespace metameric