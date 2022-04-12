#include <metameric/gl/buffer.h>
#include <metameric/gl/enums.h>
#include <glad/glad.h>

namespace metameric::gl {
  Buffer::Buffer(size_t size, const void *data, BufferStorageFlags flags)
  : Handle<>(true) {
    glCreateBuffers(1, &object());
    glNamedBufferStorage(object(), size, data, static_cast<uint>(flags));
  }

  Buffer::~Buffer() {
    if (!is_init()) {
      return;
    }
    glDeleteBuffers(1, &object());
  }
} /* namespace metameric::gl */
