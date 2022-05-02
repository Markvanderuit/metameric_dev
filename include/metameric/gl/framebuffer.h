#pragma once

#include <metameric/gl/detail/fwd.h>
#include <metameric/gl/detail/handle.h>
#include <initializer_list>
#include <span>

namespace metameric::gl {
  struct FramebufferAttachmentCreateInfo {
    FramebufferAttachmentType type;
    const Handle<> &texture;
    uint index = 0;
    uint level = 0;
  };

  class Framebuffer : public Handle<> {
    using Base = Handle<>;

  public:
    /* constr/destr */

    Framebuffer() = default;
    Framebuffer(FramebufferAttachmentCreateInfo info);
    Framebuffer(std::initializer_list<FramebufferAttachmentCreateInfo> info);
    ~Framebuffer();

    /* state management */

    void bind() const;
    void unbind() const;

    void clear(FramebufferClearType type, 
               std::span<std::byte> data,
               uint index = 0);

    /* miscellaneous */  

    // Returns an uninitialized framebuffer to act as default framebuffer.
    static Framebuffer default_framebuffer() { return Framebuffer(); }

    inline void swap(Framebuffer &o) {
      using std::swap;
      Base::swap(o);
      // swap(_loc, o._loc);
    }

    inline bool operator==(const Framebuffer &o) const {
      return Base::operator==(o); // && _loc == o._loc;
    }

    MET_NONCOPYABLE_CONSTR(Framebuffer);
  };
} // namespace metameric::gl