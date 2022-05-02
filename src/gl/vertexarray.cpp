#include <metameric/gl/vertexarray.h>
#include <metameric/gl/detail/assert.h>

namespace metameric::gl {
  Vertexarray::Vertexarray(VertexarrayCreateInfo info)
  : Handle<>(true), _has_elements(info.elements) {
    guard(_is_init);
    glCreateVertexArrays(1, &_object);

    // Bind vertex buffer objects to vao
    for (const auto &info : info.buffers) {
      glVertexArrayVertexBuffer(_object,
                                info.binding,
                                info.buffer->object(),
                                info.offset, 
                                info.stride);
    }

    // Bind element buffer object to vao, if exists
    if (_has_elements) {
      glVertexArrayElementBuffer(_object, info.elements->object());
    }

    // Set vertex attrib formats and their bindings
    for (const auto &info : info.attribs) {
      glEnableVertexArrayAttrib(_object, info.attrib_binding);
      glVertexArrayAttribFormat(_object, 
                                info.attrib_binding,
                                (uint) info.format_size,
                                (uint) info.format_type,
                                info.normalize,
                                info.relative_offset);
      glVertexArrayAttribBinding(_object, info.attrib_binding, info.buffer_binding);
    }
  }

  Vertexarray::~Vertexarray() {
    guard(_is_init);
    glDeleteVertexArrays(1, &_object);
  }

  void Vertexarray::bind() const {
    glBindVertexArray(_object);
  }

  void Vertexarray::unbind() const {
    glBindVertexArray(0);
  }
} // namespace metameric::gl
