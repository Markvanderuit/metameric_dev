#include <metameric/gl/framebuffer.h>
#include <metameric/gl/detail/assert.h>

namespace metameric::gl {
  Framebuffer::Framebuffer(FramebufferAttachmentCreateInfo info)
  : Framebuffer({info}) { }

  Framebuffer::Framebuffer(std::initializer_list<FramebufferAttachmentCreateInfo> info)
  : Base(true) {
    guard(_is_init);
    glCreateFramebuffers(1, &_object);

    for (const auto &info : info) {
      glNamedFramebufferTexture(_object, 
        (uint) info.type + info.index, 
        info.texture.object(), 
        info.level);
    }

    uint status = glCheckNamedFramebufferStatus(_object, GL_FRAMEBUFFER);
    runtime_assert(status == GL_FRAMEBUFFER_COMPLETE,  "Framebuffer(...) construction failed");
  }

  Framebuffer::~Framebuffer() {
    guard(_is_init);
    glDeleteFramebuffers(1, &_object);
  }

  void Framebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, _object);
  }

  void Framebuffer::unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  void Framebuffer::clear(FramebufferClearType type, std::span<std::byte> data, uint index) {
    switch (type) {
    case FramebufferClearType::eColor:
      glClearNamedFramebufferfv(_object, (uint) type, index, (float *) data.data());
      break;
    case FramebufferClearType::eDepth:
      glClearNamedFramebufferfv(_object, (uint) type, 0, (float *) data.data());
      break;
    case FramebufferClearType::eStencil:
      glClearNamedFramebufferiv(_object, (uint) type, 0, (int *) data.data());
      break;
    }
  }
} // namespace metameric::gl
