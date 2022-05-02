#pragma once

#include <metameric/gl/detail/fwd.h>
#include <metameric/gl/detail/handle.h>
#include <metameric/gl/buffer.h>
#include <vector>

namespace metameric::gl {
  struct VertexBufferInfo {
    const Buffer *buffer;
    uint binding = 0;
    size_t offset = 0;
    size_t stride = 4;
  };

  struct VertexAttribInfo {
    uint attrib_binding;
    uint buffer_binding;
    VertexFormatType format_type;
    VertexFormatSize format_size = VertexFormatSize::e1;
    size_t relative_offset = 0;
    bool normalize = false;
  };

  struct VertexarrayCreateInfo {
    std::vector<VertexBufferInfo> buffers;
    std::vector<VertexAttribInfo> attribs;
    const Buffer *elements = nullptr;
  };
  
  class Vertexarray : public Handle<> {
    using Base = Handle<>;

    bool _has_elements;

  public:
    /* constr/destr */
    
    Vertexarray() = default;
    Vertexarray(VertexarrayCreateInfo info);
    ~Vertexarray();

    /* state management */

    void bind() const;
    void unbind() const;

    /* miscellaneous */

    inline bool has_elements() const { return _has_elements; }

    inline void swap(Vertexarray &o) {
      using std::swap;
      Base::swap(o);
      swap(_has_elements, o._has_elements);
    }

    inline bool operator==(const Vertexarray &o) const {
      return Base::operator==(o) && _has_elements == o._has_elements;
    }

    MET_NONCOPYABLE_CONSTR(Vertexarray);
  };
} // namespace metameric::gl
