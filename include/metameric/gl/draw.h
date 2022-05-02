#pragma once

#include <metameric/gl/detail/fwd.h>
#include <metameric/gl/detail/handle.h>
#include <metameric/gl/vertexarray.h>
#include <initializer_list>
#include <optional>

namespace metameric::gl {
  struct DrawInfo {
    PrimitiveType type;
    const Vertexarray *array = nullptr;
    uint vertex_count = 0;
    uint vertex_first = 0;
    uint instance_count = 0;
    uint vertex_base = 0;
    uint instance_base = 0;
  };

  struct DrawIndirectInfo {
    PrimitiveType type;
    const Vertexarray &array;
    const Buffer &indirect_buffer;
  };

  void draw(DrawInfo info);
  void draw(DrawIndirectInfo info);
                      
  namespace state {
    class scoped_set {
      DrawCapability _capability;
      bool _prev, _curr;
      
    public:
      scoped_set(DrawCapability capability, bool enabled);
      ~scoped_set();
    };

    template <DrawCapability C, bool B>
    class ScopedSet {
      bool _prev;

    public:
      ScopedSet();
      ~ScopedSet();
    };

    void set(DrawCapability capability, bool enabled);
    bool get(DrawCapability capability);

    void set_op(BlendOp src_operand, BlendOp dst_operand);
    void set_op(LogicOp operand);
    
    void set_viewport(Array2i size, Array2i offset = Array2i::Zero());
  } // namespace state
} // namespace metameric::gl