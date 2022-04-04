#include <metameric/core/opengl.h>
#include <glad/glad.h>

namespace metameric {

GLBuffer::GLBuffer(void const * data, size_t size)
: GLObject(size > 0), _size(size) {
  guard(_is_init);
  glCreateBuffers(1, &_handle);
  glNamedBufferStorage(_handle, _size, data, GL_DYNAMIC_STORAGE_BIT); // TODO flags!
}

GLBuffer::~GLBuffer() {
  guard(_is_init);
  glDeleteBuffers(1, &_handle);
} 

void GLBuffer::set_data(void const *data, size_t size, size_t offset) {
  printf("ptr %p\n", data);
  printf("size %u\n", size);
  printf("offset %u\n", offset);
  glNamedBufferSubData(_handle, offset, size, data);
}

void GLBuffer::get_data(void * data, size_t size, size_t offset) const {
  glGetNamedBufferSubData(_handle, offset, size, data);
}

void GLBuffer::fill_data(void const *data, size_t stride, size_t size, size_t offset) {
  auto internal_format = GL_R32UI;
  auto format = GL_RED_INTEGER;
  switch (stride) {
  case 2:
    internal_format = GL_RG32UI;
    format = GL_RG_INTEGER;
    break;
  case 3:
    internal_format = GL_RGB32UI;
    format = GL_RGB_INTEGER;
    break;
  case 4:
    internal_format = GL_RGBA32UI;
    format = GL_RGBA_INTEGER;
    break;
  default:
    break;
  }
  
  if (size > 0) {
    glClearNamedBufferSubData(_handle, internal_format, offset, size, format, GL_UNSIGNED_INT, data);
  } else {
    glClearNamedBufferData(_handle, internal_format, format, GL_UNSIGNED_INT, data);
  }
}

void GLBuffer::clear(size_t size, size_t offset) {
  fill_data(nullptr, 1, size, offset);
}


/* template <typename T>
TestBuffer<T>::TestBuffer(size_t size, void const * data)
: GLObject(size > 0), _size(size) {
  guard(_is_init);
  glCreateBuffers(1, &_handle);
  glNamedBufferStorage(_handle, size * sizeof(T), data, 0); // TODO flags!
}

template <typename T>
TestBuffer<T>::~TestBuffer() {
  guard(_is_init);
  glDeleteBuffers(1, &_handle);
} 

// Explicit template instantiation
template class TestBuffer<int>;
template class TestBuffer<uint>;
template class TestBuffer<float>;
template class TestBuffer<vec2>; */
}