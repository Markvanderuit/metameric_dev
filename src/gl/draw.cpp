#include <metameric/gl/draw.h>
#include <metameric/gl/vertexarray.h>
#include <metameric/gl/detail/assert.h>
#include <ranges>

namespace metameric::gl {
  void draw(DrawInfo info) {
    info.array->bind();
    if (info.array->has_elements()) {
      if (info.instance_count > 0) {
        glDrawElementsInstancedBaseVertexBaseInstance(
          (uint) info.type, info.vertex_count, GL_UNSIGNED_INT, 
          (void *) (sizeof(uint) * info.vertex_first), 
          info.instance_count,  info.vertex_base, info.instance_base);
      } else {
        glDrawElementsBaseVertex(
          (uint) info.type, info.vertex_count, GL_UNSIGNED_INT,
          (void *) (sizeof(uint) * info.vertex_first), info.vertex_base);
      }
    } else {
      if (info.instance_count > 0) {
        glDrawArraysInstancedBaseInstance(
          (uint) info.type, info.vertex_first, info.vertex_count, 
          info.instance_count, info.instance_base);
      } else {
        glDrawArrays((uint) info.type, info.vertex_first, info.vertex_count);
      }
    }
  }

  void draw(DrawIndirectInfo info) {
    info.array.bind();
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 
      info.indirect_buffer.object());
    if (info.array.has_elements()) {
      glDrawElementsIndirect((uint) info.type, GL_UNSIGNED_INT, nullptr);
    } else {
      glDrawArraysIndirect((uint) info.type, nullptr);
    }
  }

  namespace state {
    scoped_set::scoped_set(DrawCapability capability, bool enabled)
    : _capability(capability), _prev(get(capability)), _curr(enabled) {
      guard(_curr != _prev);
      set(_capability, _curr);
    }

    scoped_set::~scoped_set() {
      guard(_curr != _prev);
      set(_capability, _prev);
    }

    template <DrawCapability C, bool B>
    ScopedSet<C, B>::ScopedSet() : _prev(get(C)) {
      guard(B != _prev);
      set(C, B);
    };

    template <DrawCapability C, bool B>
    ScopedSet<C, B>::~ScopedSet() {
      guard(B != _prev);
      set(C, _prev);
    }

    void set(DrawCapability capability, bool enabled) {
      if (enabled) {
        glEnable((uint) capability);
      } else {
        glDisable((uint) capability);
      }
    }

    bool get(DrawCapability capability) {
      return glIsEnabled((uint) capability);
    }

    void set_op(BlendOp src_operand, BlendOp dst_operand) {
      glBlendFunc((uint) src_operand, (uint) dst_operand);
    }

    void set_op(LogicOp operand) {
      glLogicOp((uint) operand);
    }

    void set_viewport(Array2i size, Array2i offset) {
      glViewport(offset[0], offset[1], size[0], size[1]);
    }
    
    // Explicit template instantiations
    template class state::ScopedSet<DrawCapability::eCullFace, true>;
    template class state::ScopedSet<DrawCapability::eFramebufferSRGB, true>;
    template class state::ScopedSet<DrawCapability::eMultisample, true>;
    template class state::ScopedSet<DrawCapability::eDebugOutput, true>;
    template class state::ScopedSet<DrawCapability::eDebugOutputSync, true>;
    template class state::ScopedSet<DrawCapability::eBlendOp, true>;
    template class state::ScopedSet<DrawCapability::eLogicOp, true>;
    template class state::ScopedSet<DrawCapability::eDepthClamp, true>;
    template class state::ScopedSet<DrawCapability::eDepthTest, true>;
    template class state::ScopedSet<DrawCapability::eStencilTest, true>;
    template class state::ScopedSet<DrawCapability::eScissorTest, true>;
    template class state::ScopedSet<DrawCapability::eLineSmooth, true>;
    template class state::ScopedSet<DrawCapability::ePolySmooth, true>;
    template class state::ScopedSet<DrawCapability::eCullFace, false>;
    template class state::ScopedSet<DrawCapability::eFramebufferSRGB, false>;
    template class state::ScopedSet<DrawCapability::eMultisample, false>;
    template class state::ScopedSet<DrawCapability::eDebugOutput, false>;
    template class state::ScopedSet<DrawCapability::eDebugOutputSync, false>;
    template class state::ScopedSet<DrawCapability::eBlendOp, false>;
    template class state::ScopedSet<DrawCapability::eLogicOp, false>;
    template class state::ScopedSet<DrawCapability::eDepthClamp, false>;
    template class state::ScopedSet<DrawCapability::eDepthTest, false>;
    template class state::ScopedSet<DrawCapability::eStencilTest, false>;
    template class state::ScopedSet<DrawCapability::eScissorTest, false>;
    template class state::ScopedSet<DrawCapability::eLineSmooth, false>;
    template class state::ScopedSet<DrawCapability::ePolySmooth, false>;
  } // namespace state
} // namespace metameric::gl