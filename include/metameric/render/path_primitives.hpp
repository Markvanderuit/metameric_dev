#pragma once

#include <metameric/render/ray_primitives.hpp>

namespace met {
  class PathLogicPrimitive : public detail::BaseInplaceBufferPrimitive {
    gl::Program             m_program;
    DispatchDividePrimitive m_prim_ddiv;
    
  public:
    PathLogicPrimitive();

    virtual void invoke(
      const gl::Buffer &input, 
            gl::Buffer &output,
            size_t      offs = 0, 
            size_t      size = 0) override;

    virtual void invoke(
      const gl::Buffer &input, 
            gl::Buffer &output,
      const gl::Buffer &count) override;
  };

  class PathShadingPrimitive : public detail::BaseInplaceBufferPrimitive {
    gl::Program             m_program;
    DispatchDividePrimitive m_prim_ddiv;

  public:
    PathShadingPrimitive();

    virtual void invoke(
      const gl::Buffer &input, 
            gl::Buffer &output,
            size_t      offs = 0, 
            size_t      size = 0) override;

    virtual void invoke(
      const gl::Buffer &input, 
            gl::Buffer &output,
      const gl::Buffer &count) override;
  };
} // namespace met