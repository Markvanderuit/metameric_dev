#include <metameric/gl/buffer.h>
#include <metameric/gl/detail/assert.h>
#include <glad/glad.h>
#include <iostream>

namespace metameric::gl {
  Buffer::Buffer(BufferCreateInfo info)
  : Handle<>(true),
    _size(info.size) {
    guard(_is_init);
    runtime_assert(_size >= info.data.size_bytes(),
      "Buffer::Buffer(...), buffer byte size smaller than data byte size");
    glCreateBuffers(1, &_object);
    glNamedBufferStorage(object(), _size,info.data.data(),(uint) info.flags);
  }

  Buffer::~Buffer() {  
    guard(_is_init);
    glDeleteBuffers(1, &object());
  }

  void Buffer::get(std::span<std::byte> data, size_t size, size_t offset) const {
    size_t safe_size = (size == 0) ? _size : size;
    glGetNamedBufferSubData(_object, offset, safe_size, data.data());
  }

  void Buffer::set(std::span<std::byte> data, size_t size, size_t offset) {
    size_t safe_size = (size == 0) ? _size : size;
    glNamedBufferSubData(_object, offset, safe_size, data.data());
  }
  
  void Buffer::clear(std::span<std::byte> data, size_t stride, size_t size, size_t offset) {
    size_t safe_size = (size == 0) ? _size : size;
    
    auto intr_fmt = GL_R32UI, fmt = GL_RED_INTEGER, type = GL_UNSIGNED_INT;
    switch (stride) {
      case 2: intr_fmt = GL_RG32UI; fmt = GL_RG_INTEGER; break;
      case 3: intr_fmt = GL_RGB32UI; fmt = GL_RGB_INTEGER; break;
      case 4: intr_fmt = GL_RGBA32UI; fmt = GL_RGBA_INTEGER; break;
      default: break;
    }
    
    glClearNamedBufferSubData(_object, intr_fmt, offset, safe_size, fmt, type, data.data());
  }

  void Buffer::bind_to(BufferTarget target, uint index, size_t size, size_t offset) const {
    size_t safe_size = (size == 0) ? _size : size;
    glBindBufferRange((uint) target, index, _object, offset, safe_size);
  }
  
  std::span<std::byte> Buffer::map(size_t size, size_t offset, BufferAccessFlags flags) {
    size_t safe_size = (size == 0) ? _size : size;
    _is_mapped  = true;
    void *data = glMapNamedBufferRange(_object, offset, safe_size, (uint) flags);
    return { reinterpret_cast<std::byte *>(data), safe_size };
  }

  void Buffer::flush(size_t size, size_t offset) {
    guard(_is_mapped);
    size_t safe_size = (size == 0) ? _size : size;
    glFlushMappedNamedBufferRange(_object, offset, safe_size);
  }

  void Buffer::unmap() {
    _is_mapped  = false;
    glUnmapNamedBuffer(_object);
  }
} /* namespace metameric::gl */
