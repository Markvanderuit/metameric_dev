#include <metameric/core/opengl.h>
#include <glad/glad.h>

namespace metameric {

GLBuffer::GLBuffer(size_t size, void const * data)
: GLObject(size > 0) {
  guard(_is_init);
  glCreateBuffers(1, &_handle);
  glNamedBufferStorage(_handle, size, data, 0); // TODO flags
}

GLBuffer::~GLBuffer() {
  guard(_is_init);
  glDeleteBuffers(1, &_handle);
} 

}