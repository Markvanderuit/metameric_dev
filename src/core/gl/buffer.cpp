#include <metameric/core/gl/buffer.h>
#include <metameric/core/gl/exception.h>
#include <array>
#include <type_traits>
#include <glad/glad.h>

using namespace metameric;
using namespace metameric::gl;

template <typename T, uint N>
struct FlagMap {
  constexpr
  FlagMap(std::array<T, N> i, std::array<uint, N> o)  : _inputs(i), _outputs(o) { }

  constexpr uint map(uint input) const {
    uint output = 0u;
    for (uint i = 0; i < N; ++i) {
      if (has_flag(input, _inputs[i])) {
        output |= _outputs[i];
      }
    }
    return output;
  }

  std::array<T, N> _inputs;
  std::array<uint, N> _outputs;
};

constexpr FlagMap<BufferStorageFlags, 2> _storage_flags_map({ 
  BufferStorageFlags::eDynamic, BufferStorageFlags::eClient
}, { 
  GL_DYNAMIC_STORAGE_BIT, GL_CLIENT_STORAGE_BIT
});

constexpr FlagMap<BufferMappingFlags, 4> _mappings_flags_map({ 
  BufferMappingFlags::eRead, BufferMappingFlags::eWrite,
  BufferMappingFlags::ePersistent, BufferMappingFlags::eCoherent
}, { 
  GL_MAP_READ_BIT, GL_MAP_WRITE_BIT, GL_MAP_PERSISTENT_BIT, GL_MAP_COHERENT_BIT
});

Buffer::Buffer(size_t size, void const *ptr, uint storage_flags, uint mapping_flags)
: AbstractObject(size > 0), _size(size), _storage_flags(storage_flags), 
  _mapping_constr_flags(mapping_flags), _mapping_access_flags(0u) {
  guard(_is_init);
  auto flags = _storage_flags_map.map(storage_flags) | _mappings_flags_map.map(mapping_flags);
  glCreateBuffers(1, &_handle);
  glNamedBufferStorage(_handle, _size, ptr, flags);
  gl_assert("Buffer::Buffer(...)");
}

Buffer::~Buffer() {
  guard(_is_init);
  glDeleteBuffers(1, &_handle);
  gl_assert("Buffer::~Buffer(...)");
}

void Buffer::get(void *ptr, size_t size, size_t offset) const {
  runtime_assert(size == 0 || (offset + size) <= _size, 
    "Buffer::get(...), requested size exceeds buffer size");
  glGetNamedBufferSubData(_handle, offset, size, ptr);
  gl_assert("Buffer::get(...)");
}

void Buffer::set(void const *ptr, size_t size, size_t offset) {
  runtime_assert(size == 0 || (offset + size) <= _size, 
    "Buffer::set(...), requested size exceeds buffer size");
  runtime_assert(has_flag(_storage_flags, BufferStorageFlags::eDynamic),
    "Buffer::set(...), buffer does not have dynamic flag set");
  glNamedBufferSubData(_handle, offset, size, ptr);
  gl_assert("Buffer::set(...)");
}

void Buffer::fill(void const *ptr, size_t stride, size_t size, size_t offset) {
  runtime_assert(size == 0 || (offset + size <= _size), 
    "Buffer::fill(...), requested size exceeds buffer size");

  // Given integer mapping, no conversion of any uploaded data is performed
  auto intr_fmt = GL_R32UI, fmt = GL_RED_INTEGER, type = GL_UNSIGNED_INT;
  switch (stride) {
    case 2: intr_fmt = GL_RG32UI; fmt = GL_RG_INTEGER; break;
    case 3: intr_fmt = GL_RGB32UI; fmt = GL_RGB_INTEGER; break;
    case 4: intr_fmt = GL_RGBA32UI; fmt = GL_RGBA_INTEGER; break;
    default: break;
  }
    
  if (size != 0) {
    glClearNamedBufferSubData(_handle, intr_fmt, offset, size, fmt, type, ptr);
  } else {
    glClearNamedBufferData(_handle, intr_fmt, fmt, type, ptr);
  }
  
  gl_assert("Buffer::fill(...)");
}

void Buffer::clear(size_t size, size_t offset) {
  fill(nullptr, 1, size, offset);
}

