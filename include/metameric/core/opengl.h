#pragma once

// #include <array>
#include <initializer_list>
#include <tuple>
#include <span>
#include <utility>
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

// struct vec2 {
//   float x, y;
// };

/* template <typename T>
class TestBuffer : public GLObject {
  using Base = GLObject;  

  size_t _size;

public:
  TestBuffer() = default;
  ~TestBuffer();
  
  TestBuffer(size_t size, void const *data = nullptr);
  // TestBuffer(size_t size) : TestBuffer(nullptr, size) { }
  TestBuffer(const std::vector<T> &v) : TestBuffer(v.size(), v.data()) { }
  TestBuffer(const std::initializer_list<T> &l) : TestBuffer(l.size(), l.begin()) { }
  
  template <typename T_>
  TestBuffer(TestBuffer<T_> &&o) noexcept {
    swap(reinterpret_cast<TestBuffer<T> &&>(o));
    _size = (_size * sizeof(T_)) / sizeof(T);
  } 

  template <typename T_>
  inline
  TestBuffer<T> & operator=(TestBuffer<T_> &&o) noexcept {
    swap(reinterpret_cast<TestBuffer<T> &&>(o));
    _size = (_size * sizeof(T_)) / sizeof(T);
    return *this;
  }

  inline size_t size() const { return _size; }

  inline
  void swap(TestBuffer<T> &o) {
    using std::swap;
    Base::swap(o);
    swap(_size, o._size);
  }

  inline bool operator==(const TestBuffer<T> &o) const {
    using std::tie;
    return Base::operator==(o)
        && tie(_size)
        == tie(o._size);
  }

  MET_CLASS_NON_COPYABLE_CONSTR(TestBuffer<T>)
}; */

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

  /* void set_data(void const *data, size_t size = 0, size_t offset = 0);
  void get_data(void * data, size_t size = 0, size_t offset = 0);
  void fill_data(void const *data, size_t stride = 1, size_t size = 0, size_t offset = 0);
  void clear_data(size_t size = 0, size_t offset = 0); */

  /* template <typename T>
  void fill_data(const T *t, size_t stride = 1, size_t size = 0, size_t offset = 0) {
    
  } */

  /* template <typename T>
  void set_data(const T &t, size_t size = 0, size_t offset = 0) {
    if (size != 0 && offset != 0) {
      set_data(t.data(), size, offset);
    } else {
      set_data(t.data(), t.size() * sizeof(T::value_type));
    }
  } */

  /* template <typename T>
  std::vector<T> get_data(size_t size = 0, size_t offset = 0) {
    if (size != 0 && offset != 0) {
      std::vector<T> v(size);
      get_data(v.data(), size, offset);
      return v;
    } else {
      std::vector<T> v(_size / sizeof(T));
      get_data(v.data(), _size);
      return v;
    }
  } */

  /* template <typename T>
  void get_data(const std::initializer_list<T> &data, size_t size = 0, size_t offset = 0) {
    if (size != 0 || offset != 0) {
      get_data(data.begin(), size, offset);
    } else {
      get_data(data.begin(), data.size() * sizeof(T));
    }
  } */

  /* template <typename T>
  void get_data(T &t, size_t size = 0, size_t offset = 0) {
    if (size != 0 || offset != 0) {
      get_data(t.data(), size, offset);
    } else {
      get_data(t.data(), t.size() * sizeof(T::value_type));
    }
  } */


  /* template <typename T>
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

  void bind_to(GLBufferTarget target); */

public:
  // Base constructors to setup/teardown resources
  GLBuffer() = default;
  GLBuffer(void const *data, size_t size);
  ~GLBuffer();

  // Convenience constructors
  GLBuffer(size_t size) 
  : GLBuffer(nullptr, size) { }
  template <typename T>
  GLBuffer(std::initializer_list<const T> c)
  : GLBuffer(std::data(c), c.size() * sizeof(T)) { }
  template <typename T, size_t E>
  GLBuffer(std::span<const T, E> c) 
  : GLBuffer(std::data(c), c.size() * sizeof(T)) { }
  template <typename C>
  GLBuffer(const C &c)
  : GLBuffer(std::data(c), c.size() * sizeof(typename C::value_type)) { }
  
  // Swap in-place with other object
  inline void swap(GLBuffer &o) {
    Base::swap(o);
    using std::swap;
    swap(_size, o._size);
  }
  
  // Comparison operator checks underlying handle in superclass
  inline bool operator==(const GLBuffer &o) const {
    using std::tie;
    return Base::operator==(o)
        && tie(_size)
        == tie(o._size);
  }

  // Object is movable, but not directly copyable
  MET_CLASS_NON_COPYABLE_CONSTR(GLBuffer)

public:
  // Direct get/set/fill/clear functions
  void get_data(void *data, size_t size = 0, size_t offset = 0) const;
  void set_data(void const *data, size_t size = 0, size_t offset = 0);
  void fill_data(void const *data, size_t stride = 1, size_t size = 0, size_t offset = 0);
  void clear(size_t size = 0, size_t offset = 0);

  // Convenience set overrides
  template <typename T, size_t E>
  void get(std::span<T, E> c) { get_data(std::data(c), c.size() * sizeof(T)); }
  template <typename C>
  void get(C &c) { get_data(std::data(c), c.size() * sizeof(typename C::value_type)); }

  // Convenience get overrides
  // template <typename T>
  // void set(std::initializer_list<T> c) { set_data(std::data(c), c.size() * sizeof(T)); }
  // template <typename T, size_t E>
  // void set(std::span<T, E> c) { set_data(std::data(c), c.size() * sizeof(T)); }
  template <typename C>
  void set(const C &c) { set_data(std::data(c), c.size() * sizeof(typename C::value_type)); }

  // Constructing accessor for vector/list containers
  template <typename Cont>
  Cont get_as() { 
    Cont c(size() / sizeof(typename Cont::value_type));
    get(c);
    return c;
  }

  // Convenience data operators
  // template <typename T>
  // inline
  // void set_data(const std::vector<T> &v)

private:
  size_t _size;
};

} // namespace metameric