#include <metameric/gl/draw.h>
#include <metameric/gl/program.h>
#include <metameric/gl/sync.h>
#include <metameric/gl/utility.h>
#include <metameric/gl/vertexarray.h>
#include <metameric/gl/detail/assert.h>

namespace metameric::gl {
  void draw(DrawInfo info) {
    runtime_assert(info.array,
      "gl::draw(...), DrawInfo submitted without array object");
    info.array->bind();

    if (info.program) info.program->bind();

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
    runtime_assert(info.array,
      "gl::draw(...), DrawIndirectInfo submitted without array object");
    info.array->bind();

    if (info.program) info.program->bind();

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, info.buffer->object());
    memory_barrier(gl::BarrierFlags::eIndirectBuffer);
    
    if (info.array->has_elements()) {
      glDrawElementsIndirect((uint) info.type, GL_UNSIGNED_INT, nullptr);
    } else {
      glDrawArraysIndirect((uint) info.type, nullptr);
    }
  }

  
  void compute(ComputeInfo info) {
    if (info.program) info.program->bind();
    glDispatchCompute(info.groups_x, info.groups_y, info.groups_z);
  }

  void compute(ComputeIndirectInfo info) {
    if (info.program) info.program->bind();

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, info.buffer->object());
    memory_barrier(gl::BarrierFlags::eIndirectBuffer);
        
    glDispatchComputeIndirect(0);
  }
} // namespace metameric::gl