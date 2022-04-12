#pragma once

#include <utility>
#include <tuple>
#include <metameric/core/define.h>
#include <metameric/core/fwd.h>

namespace metameric::gl {
  /**
   * Thin abstract class wrapping OpenGL objects, which holds
   * a resource handle and signals initialization of said resource.
   */
  struct AbstractObject {
    // Handle to the OpenGL object which this object owns
    inline uint handle() const { return _handle; }

    // Whether the OpenGL object is initialized in any way
    inline bool is_init() const { return _is_init; }
    
  protected:
    bool _is_init;
    uint _handle;

  protected:
    // Hidden constructor, virtual destructor for abstract object
    explicit AbstractObject(bool init = false) noexcept : _is_init(init), _handle(0u) { }
    virtual ~AbstractObject() = default;

    inline
    void swap(AbstractObject &o) {
      using std::swap;
      swap(_is_init, o._is_init);
      swap(_handle, o._handle);
    }

    inline
    bool operator==(const AbstractObject &o) const {
      using std::tie;
      return tie(_is_init, _handle) 
          == tie(o._is_init, o._handle);
    }
    
    MET_DECLARE_NONCOPYABLE(AbstractObject);
  };

} // namespace metameric::gl