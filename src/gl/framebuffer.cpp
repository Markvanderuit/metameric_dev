#include <metameric/gl/framebuffer.h>
#include <metameric/gl/detail/assert.h>

namespace metameric::gl {
  namespace detail {
    GLenum framebuffer_attachment(gl::FramebufferAttachmentType type) {
      switch (type) {
        case gl::FramebufferAttachmentType::eDepth: return GL_DEPTH_ATTACHMENT;
        case gl::FramebufferAttachmentType::eStencil: return GL_STENCIL_ATTACHMENT;
        default: return GL_COLOR_ATTACHMENT0;
      }
    }
  } // namespace detail

  Framebuffer::Framebuffer(FramebufferAttachmentCreateInfo info)
  : Framebuffer({info}) { }

  Framebuffer::Framebuffer(std::initializer_list<FramebufferAttachmentCreateInfo> info)
  : Base(true) {
    guard(_is_init);
    glCreateFramebuffers(1, &_object);

    for (const auto &info : info) {
      glNamedFramebufferTexture(_object, 
        detail::framebuffer_attachment(info.type) + info.index, 
        info.texture->object(), 
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

  #define MET_IMPL_CLEAR(type, type_short)\
    template <> void Framebuffer::clear<type>\
    (FramebufferAttachmentType t, type v, uint i)\
    { glClearNamedFramebuffer ## type_short ## v(_object, (uint) t, i, &v); }\
    template <> void Framebuffer::clear<eig::Array<type, 2, 1>>\
    (FramebufferAttachmentType t, eig::Array<type, 2, 1> v, uint i)\
    { glClearNamedFramebuffer ## type_short ## v(_object, (uint) t, i, v.data()); }\
    template <> void Framebuffer::clear<eig::Array<type, 3, 1>>\
    (FramebufferAttachmentType t, eig::Array<type, 3, 1> v, uint i)\
    { glClearNamedFramebuffer ## type_short ## v(_object, (uint) t, i, v.data()); }\
    template <> void Framebuffer::clear<eig::Array<type, 4, 1>>\
    (FramebufferAttachmentType t, eig::Array<type, 4, 1> v, uint i)\
    { glClearNamedFramebuffer ## type_short ## v(_object, (uint) t, i, v.data()); }

  // Explicit template specializations
  MET_IMPL_CLEAR(float, f)
  MET_IMPL_CLEAR(uint, ui)
  MET_IMPL_CLEAR(int, i)
} // namespace metameric::gl
