#include <metameric/gl/sync.h>
#include <metameric/gl/detail/assert.h>

namespace metameric::gl {
  void memory_barrier(gl::BarrierFlags flags) {
    glMemoryBarrier((uint) flags);
  }

  Fence::Fence()
  : Base(true) {
    guard(_is_init);
    _object = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  }

  Fence::~Fence() {
    guard(_is_init);
    glDeleteSync((GLsync) _object);
  }

  void Fence::cpu_wait_sync() {
    glClientWaitSync((GLsync) _object, 0, 1'000'000'000);
  }

  void Fence::gpu_wait_sync() {
    glWaitSync((GLsync) _object, 0, GL_TIMEOUT_IGNORED);
  }
} // namespace metameric::gl