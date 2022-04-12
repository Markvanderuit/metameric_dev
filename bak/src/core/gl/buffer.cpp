#include <metameric/core/gl/detail/enum.h>
#include <metameric/core/gl/detail/exception.h>
#include <metameric/core/gl/buffer.h>
#include <glad/glad.h>

using namespace metameric;
using namespace metameric::gl;

/* constexpr EnumMap<BufferTarget, 4> _target_map({
  { BufferTarget::eAtomicCounter, GL_ATOMIC_COUNTER_BUFFER },
  { BufferTarget::eShaderStorage, GL_SHADER_STORAGE_BUFFER },
  { BufferTarget::eTransformFeedback, GL_TRANSFORM_FEEDBACK_BUFFER },
  { BufferTarget::eUniform, GL_UNIFORM_BUFFER },
}); */

/* constexpr EnumMap<BufferStorageFlags, 2> _storage_flags_map({
  { BufferStorageFlags::eDynamic, GL_DYNAMIC_STORAGE_BIT },
  { BufferStorageFlags::eClient, GL_CLIENT_STORAGE_BIT },
});

constexpr EnumMap<BufferMappingFlags, 4> _mappings_flags_map({
  { BufferMappingFlags::eRead, GL_MAP_READ_BIT },
  { BufferMappingFlags::eWrite, GL_MAP_WRITE_BIT },
  { BufferMappingFlags::ePersistent, GL_MAP_PERSISTENT_BIT },
  { BufferMappingFlags::eCoherent, GL_MAP_COHERENT_BIT }
}); */

Buffer::Buffer(size_t size, void const *ptr, BufferStorageFlags storage_flags, BufferMappingFlags mapping_flags)
: AbstractObject(size > 0), _size(size), _storage_flags(storage_flags), 
  _mapping_constr_flags(mapping_flags), _mapping_access_flags(BufferMappingFlags::eNone) {
  guard(_is_init);
  // uint flags = _storage_flags_map.map(storage_flags) | _mappings_flags_map.map(mapping_flags);

  glCreateBuffers(1, &_handle);
  glNamedBufferStorage(_handle, _size, ptr, (uint) storage_flags | (uint) mapping_flags);
  gl_assert("Buffer::Buffer(...)");
}

Buffer::~Buffer() {
  guard(_is_init);
  glDeleteBuffers(1, &_handle);
  gl_assert("Buffer::~Buffer(...)");
}

void Buffer::get_mem(void *ptr, size_t size, size_t offset) const {
  runtime_assert(size == 0 || (offset + size) <= _size, 
    "Buffer::get_mem(...), requested offset + size exceeds buffer size");

  glGetNamedBufferSubData(_handle, offset, size, ptr);
  gl_assert("Buffer::get_mem(...)");
}

void Buffer::set_mem(void const *ptr, size_t size, size_t offset) {
  runtime_assert(size == 0 || (offset + size) <= _size, 
    "Buffer::set_mem(...), requested offset + size exceeds buffer size");
  runtime_assert((uint) (_storage_flags & BufferStorageFlags::eDynamic),
    "Buffer::set_mem(...), buffer does not have dynamic flag set");
  
  glNamedBufferSubData(_handle, offset, size, ptr);
  gl_assert("Buffer::set_mem(...)");
}

void Buffer::fill_mem(void const *ptr, size_t stride, size_t size, size_t offset) {
  runtime_assert(size == 0 || (offset + size <= _size), 
    "Buffer::fill_mem(...), requested offset + size exceeds buffer size");

  // Given integer mapping, no conversion of any uploaded data is performed
  auto intr_fmt = GL_R32UI, fmt = GL_RED_INTEGER, type = GL_UNSIGNED_INT;
  switch (stride) {
    case 2: intr_fmt = GL_RG32UI; fmt = GL_RG_INTEGER; break;
    case 3: intr_fmt = GL_RGB32UI; fmt = GL_RGB_INTEGER; break;
    case 4: intr_fmt = GL_RGBA32UI; fmt = GL_RGBA_INTEGER; break;
    default: break;
  }
  
  glClearNamedBufferSubData(_handle, intr_fmt, offset, size == 0u ? _size : size, fmt, type, ptr);
  gl_assert("Buffer::fill_mem(...)");
}

void Buffer::clear(size_t size, size_t offset) {
  fill_mem(nullptr, 1, size * sizeof(uint), offset * sizeof(uint));
}

Buffer Buffer::copy(size_t size, size_t offset) const {
  runtime_assert(size == 0 || (offset + size <= _size), 
    "Buffer::copy(...), requested offset + size exceeds buffer size");

  Buffer b(_size, nullptr, _storage_flags, _mapping_constr_flags);
  copy_to(b, size, offset, 0);
  return b;
}

void Buffer::copy_from(const Buffer &o, size_t size, size_t r_offset, size_t w_offset) {
  runtime_assert(size == 0 || (r_offset + size <= o.size()), 
    "Buffer::copy_from(...), requested read offset + size exceeds buffer size");
  runtime_assert(size == 0 || (w_offset + size <= _size), 
    "Buffer::copy_from(...), requested write offset + size exceeds buffer size");

  glCopyNamedBufferSubData(o.handle(), _handle, r_offset, w_offset, size == 0u ? _size : size);
  gl_assert("Buffer::copy_from(...)");
}

void Buffer::copy_to(Buffer &o, size_t size, size_t r_offset, size_t w_offset) const {
  o.copy_from(*this, size, r_offset, w_offset);
}

void Buffer::bind_to(BufferTarget target, uint index, size_t offset, size_t size) const {
  runtime_assert(size == 0 || (offset + size <= _size), 
    "Buffer::copy_to(...), requested read offset + size exceeds buffer size");
  
  if (size != 0 || offset != 0) {
    glBindBufferRange(static_cast<uint>(target), index, _handle, offset, size);
  } else {
    glBindBufferBase(static_cast<uint>(target), index, _handle);
  }

  gl_assert("Buffer::bind_to(...)");
}