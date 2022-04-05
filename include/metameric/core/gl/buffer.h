#pragma once

#include <initializer_list>
#include <span>
#include <metameric/core/define.h>
#include <metameric/core/fwd.h>
#include <metameric/core/gl/abstract_object.h>

namespace metameric::gl {
  /**
   * This list of values classifies targets for a buffer binding operation.
   */
  enum class BufferTarget { eAtomicCounter, eShaderStorage, eTransformFeedback, eUniform };

  /**
   * This list of flags classifies the intended usage of a Buffer's data store. Note that map bits 
   * are explicitly missing and moved to the below BufferMappingFlags.
   */
  enum class BufferStorageFlags : uint {
    // No flags set (default value)
    eNone           = 0x0000,

    // The contents of the data store may be updated after creation from the OpenGL client
    eDynamic        = 0x0100,

    // The data store is local to the OpenGL client, instead of the OpenGL server
    eClient         = 0x0200
  };

  /**
   * This list of flags classifies the intended usage of a Buffer with regards to mapping.
   */
  enum class BufferMappingFlags : uint {
    // No flags set (default value)
    eNone           = 0x0000,

    // The data store may be mapped by the client for read access
    eRead           = 0x0100,

    // The data store may be mapped by the client for write access
    eWrite          = 0x0200,

    // The server may read/write to/from the data store while it is mapped
    ePersistent     = 0x0300,

    // The data store is local to the OpenGL client, instead of the OpenGL server
    eCoherent       = 0x0400
  };
  
  /**
   * Thin wrapper around an OpenGL buffer object with a number of convenience functions. Does
   * not allow copy construction or assignment.
   */
  class Buffer : public AbstractObject {
  public:
    // Size in bytes of the underlying buffer storage
    inline size_t size() const { return _size; }

    // Storage flags set for the underlying buffer storage
    inline uint storage_flags() const { return _storage_flags; }

    // Mapping flags set during buffer initialization
    inline uint mapping_constr_flags() const { return _mapping_constr_flags; }

    // Mapping flags set during buffer mapping
    inline uint mapping_access_flags() const { return _mapping_access_flags; }

    // Is the buffer currently mapped?
    inline bool is_mapped() const { return _is_mapped; }

  private:
    size_t _size;
    uint _storage_flags;
    uint _mapping_constr_flags;
    uint _mapping_access_flags;
    bool _is_mapped;
    
    template <typename T>
    size_t check_size(size_t size) const { return size > 0 ? size * sizeof(T) : _size; }

  public:
    // Base constructor to setup/teardown underlying buffer storage
    Buffer() = default;
    Buffer(size_t size, 
           void const *ptr = nullptr, 
           uint storage_flags = 0u,
           uint mapping_flags = 0u);
    ~Buffer();

    // Convenience constructors accepting common STL formats
    template <typename T>
    Buffer(std::initializer_list<const T> c, uint strg_fl = 0u, uint map_fl = 0u)
    : Buffer(c.size() * sizeof(T), std::data(c), strg_fl, map_fl) { }
    template <typename T, size_t E>
    Buffer(std::span<const T, E> c, uint strg_fl = 0u, uint map_fl = 0u) 
    : Buffer(c.size() * sizeof(T), std::data(c), strg_fl, map_fl) { }
    template <typename C>
    Buffer(const C &c, uint strg_fl = 0u, uint map_fl = 0u)
    : Buffer(c.size() * sizeof(typename C::value_type), std::data(c), strg_fl, map_fl) { }

  public:
    // Base get/set/fill operations to read/write underlying data
    void get_mem(void *ptr, size_t size = 0, size_t offset = 0) const;
    void set_mem(void const *ptr, size_t size = 0, size_t offset = 0);
    void fill_mem(void const *ptr, size_t stride = 1, size_t size = 0, size_t offset = 0);

    // Convenience get_mem() variants accepting common STL formats
    template <typename T, size_t E>
    auto get(std::span<T, E> c, size_t size = 0, size_t offset = 0) const    
    { get_mem(std::data(c), check_size<T>(size), offset * sizeof(T)); return c; }
    template <typename C>
    auto & get(C &c, size_t size = 0, size_t offset = 0) const               
    { get_mem(std::data(c), check_size<C::value_type>(size), offset * sizeof(typename C::value_type)); return c; }
    
    // Convenience function which constructs and writes directly into vector/list container
    template <typename C>
    C get_as(size_t size = 0, size_t offset = 0) const { 
      size_t checked_size = check_size<C::value_type>(size) / sizeof(typename C::value_type);
      C c(checked_size); 
      return get(c, checked_size, offset); 
    }

    // Convenience set_mem() variants accepting common STL formats
    template <typename T>
    void set(std::initializer_list<T> c, size_t size = 0, size_t offset = 0) 
    { set_mem(std::data(c), check_size<T>(size), offset * sizeof(T)); }
    template <typename T, size_t E>
    void set(std::span<T, E> c, size_t size = 0, size_t offset = 0)
    { set(std::data(c), check_size<T>(size), offset * sizeof(T)); }
    template <typename C>
    void set(const C &c, size_t size = 0, size_t offset = 0)
    { set_mem(std::data(c), check_size<C::value_type>(size), offset * sizeof(typename C::value_type)); }

    // Convenience fill_mem() variants accepting common STL formats
    template <typename T>
    void fill(std::initializer_list<T> c, size_t size = 0, size_t offset = 0)
    { fill_mem(std::data(c), check_size<T>(size), offset * sizeof(T)); }
    template <typename T, size_t E>
    void fill(std::span<T, E> c, size_t size = 0, size_t offset = 0)
    { fill_mem(std::data(c), check_size<T>(size), offset * sizeof(T)); }
    template <typename C>
    void fill(const C &c, size_t size = 0, size_t offset = 0)
    { fill_mem(std::data(c), check_size<C::value_type>(size), offset * sizeof(typename C::value_type)); }

    // Convenience clear() function which fills a subrange with all zeroes
    void clear(size_t size = 0, size_t offset = 0);

    // Copy constr/assign is deleted to prevent accidental usage, but explicit copies can still be
    // performed between (parts of) buffers, if necessary.
    Buffer copy(size_t size = 0, size_t offset = 0) const;
    void copy_from(const Buffer &o, size_t size = 0, size_t r_offset = 0, size_t w_offset = 0);
    void copy_to(Buffer &o, size_t size = 0, size_t r_offset = 0, size_t w_offset = 0) const;

    // Convenience function merging OpenGL's bindBase/bindRange
    void bind_to(BufferTarget target, uint index, size_t offset = 0, size_t size = 0) const;

    // Mapping/unmapping of buffer - note: ensure flags are set corretly
    /* void * map(size_t size = 0, size_t offset = 0);
    void flush_map();
    void wait_map();
    void unmap(); */

    // Convenience function which maps a buffer and returns a span of the mapped memory
    /* template <typename T, size_t E>
    std::span<T, E> map_as(size_t size = 0, size_t offset = 0) {
      return { };
    } */

  public:
    inline void swap(Buffer &o) {
      using std::swap;
      AbstractObject::swap(o);
      swap(_size, o._size);
      swap(_storage_flags, o._storage_flags);
      swap(_mapping_constr_flags, o._mapping_constr_flags);
      swap(_mapping_access_flags, o._mapping_access_flags);
      swap(_is_mapped, o._is_mapped);
    }
    
    inline bool operator==(const Buffer &o) const {
      using std::tie;
      return AbstractObject::operator==(o)
        && tie(_size, _storage_flags, _mapping_constr_flags, _mapping_access_flags, _is_mapped)
        == tie(o._size, o._storage_flags, _mapping_constr_flags, _mapping_access_flags, _is_mapped);
    }

    // Enable move constr/assign, but disallow direct copies to prevent accidental usage
    MET_DECLARE_NONCOPYABLE(Buffer);
  };

  MET_DECLARE_ENUM_FLAGS(BufferStorageFlags)
  MET_DECLARE_ENUM_FLAGS(BufferMappingFlags)
} // namespace metameric::gl