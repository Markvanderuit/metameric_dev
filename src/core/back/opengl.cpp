#include <metameric/core/back/opengl.h>
#include <glad/glad.h>

namespace metameric {

GLbitfield flags_from_buffer_storage_flags(uint flags) {
  // These should be the same, but it is best to translate them anyways
  GLbitfield f = 0u;
  if (has_flag(flags, GLBufferStorageFlags::eClientStorage)) f |= GL_CLIENT_STORAGE_BIT;
  if (has_flag(flags, GLBufferStorageFlags::eDynamicStorage)) f |= GL_DYNAMIC_STORAGE_BIT;
  return f;
} 

GLBuffer::GLBuffer(size_t size, void const * data, uint storage_flags)
: GLObject(size > 0), _size(size), _storage_flags(storage_flags) {
  guard(_is_init);
  glCreateBuffers(1, &_handle);
  glNamedBufferStorage(_handle, _size, data, flags_from_buffer_storage_flags(_storage_flags));
}

GLBuffer::~GLBuffer() {
  guard(_is_init);
  glDeleteBuffers(1, &_handle);
} 

void GLBuffer::set(void const *data, size_t size, size_t offset) {
  // TODO runtime_assert that size does not exceed!
  glNamedBufferSubData(_handle, offset, size, data);
}

void GLBuffer::get(void * data, size_t size, size_t offset) const {
  // TODO runtime_assert that size does not exceed!
  // TODO runtime_assert presence of dynamic or client storage flags!
  glGetNamedBufferSubData(_handle, offset, size, data);
}

void GLBuffer::fill(void const *data, size_t stride, size_t size, size_t offset) {
  // TODO runtime_assert that size does not exceed!

  // Given integer mapping, no conversion of any uploaded data is performed
  auto internal_format = GL_R32UI, format = GL_RED_INTEGER;
  switch (stride) {
    case 2: internal_format = GL_RG32UI; format = GL_RG_INTEGER; break;
    case 3: internal_format = GL_RGB32UI; format = GL_RGB_INTEGER; break;
    case 4: internal_format = GL_RGBA32UI; format = GL_RGBA_INTEGER; break;
    default: break;
  }
  
  if (size != 0) {
    glClearNamedBufferSubData(_handle, internal_format, offset, size, format, GL_UNSIGNED_INT, data);
  } else {
    glClearNamedBufferData(_handle, internal_format, format, GL_UNSIGNED_INT, data);
  }
}

void GLBuffer::clear(size_t size, size_t offset) {
  fill(nullptr, 1, size, offset);
}

GLBuffer GLBuffer::copy(size_t size, size_t offset) const {
  size_t copy_size = size == 0u ? _size : size;
  GLBuffer copy(copy_size, nullptr, _storage_flags);
  copy_to(copy, copy_size, offset, 0);
  return copy;
}

void GLBuffer::copy_from(const GLBuffer &other, size_t size, size_t r_offset, size_t w_offset) {
  size_t copy_size = size == 0u ? _size : size;
  glCopyNamedBufferSubData(other.handle(), _handle, r_offset, w_offset, copy_size);
}

void GLBuffer::copy_to(GLBuffer &other, size_t size, size_t r_offset, size_t w_offset) const {
  other.copy_from(*this, size, r_offset, w_offset);
}

void GLBuffer::bind(GLBufferTarget target, uint index, size_t offset, size_t size) const {
  if (offset != 0 || size != 0) {
    // TODO translate enum to target
    // glBindBufferRange(TARGET, index, _handle, offset, size);
  } else {
    // glBindBufferBase(TARGET, index, _handle);
  }
}

} // namespace metameric