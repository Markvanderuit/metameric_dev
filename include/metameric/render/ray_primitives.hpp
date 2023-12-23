#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/program.hpp>

namespace met {
  namespace detail {
    struct BasePrimitive {
      // pure virtual holding primitive operation
      virtual void invoke(const gl::Buffer &input, gl::Buffer &output, size_t offs = 0, size_t size = 0) = 0;
      virtual void invoke(const gl::Buffer &input, gl::Buffer &output, const gl::Buffer &work)           = 0;
      
      // operator() shorthand for invoke
      void operator()(const gl::Buffer &input, gl::Buffer &output, size_t offs = 0, size_t size = 0) {
        return invoke(input, output, offs, size);
      }

      // operator() shorthand for invoke
      void operator()(const gl::Buffer &input, gl::Buffer &output, const gl::Buffer &count) {
        return invoke(input, output, count);
      }
    };

    class BaseOutputBufferPrimitive : public BasePrimitive {
      gl::Buffer m_output;

    public:
      using BasePrimitive::invoke;
      using BasePrimitive::operator();

      const gl::Buffer &invoke(const gl::Buffer &input, size_t offs = 0, size_t size = 0) {
        if (!m_output.is_init() || m_output.size() < std::max(input.size(), size))
          m_output = {{ .size = std::max(input.size(), size) }};
        invoke(input, m_output, offs, size);
        return m_output;
      }
      
      const gl::Buffer &operator()(const gl::Buffer &input, size_t offs = 0, size_t size = 0) {
        return invoke(input, offs, size);
      }

      const gl::Buffer &get() {
        met_trace();
        return m_output;
      }

      gl::Buffer take() {
        met_trace();
        return std::move(m_output);
      }
    };

    struct BaseInplaceBufferPrimitive : public BasePrimitive {
      using BasePrimitive::invoke;
      using BasePrimitive::operator();

      gl::Buffer &invoke(gl::Buffer &input, size_t offs = 0, size_t size = 0) {
        invoke(input, input, offs, size);
        return input;
      }

      gl::Buffer &invoke(gl::Buffer &input, const gl::Buffer &count) {
        invoke(input, input, count);
        return input;
      }
      
      gl::Buffer &operator()(gl::Buffer &input, size_t offs = 0, size_t size = 0) {
        return invoke(input, offs, size);
      }
      
      gl::Buffer &operator()(gl::Buffer &input, const gl::Buffer &count) {
        return invoke(input, count);
      }
    };
  } // namespace detail

  
  // Helper primitive that takes an input buffer and generates an output
  // buffer with the input's first value divided by n, rounded up
  // Useful for generating indirect dispatch buffers
  class DispatchDividePrimitive : private detail::BasePrimitive {
    gl::Program m_program;
    gl::Buffer  m_output;

    virtual void invoke(
      const gl::Buffer &input, 
            gl::Buffer &output,
            size_t      offs = 0, 
            size_t      size = 0) override;

    virtual void invoke(
      const gl::Buffer &input, 
            gl::Buffer &output,
      const gl::Buffer &count) override;
    
  public:
    DispatchDividePrimitive(uint div);

    const gl::Buffer &invoke(const gl::Buffer &input) {
      if (!m_output.is_init())
        m_output = {{ .size = 4 * sizeof(uint) }};
      invoke(input, m_output);
      return m_output;
    }
    
    const gl::Buffer &operator()(const gl::Buffer &input) {
      return invoke(input);
    }
    
    const gl::Buffer &get() {
      met_trace();
      return m_output;
    }
    
    gl::Buffer take() {
      met_trace();
      return std::move(m_output);
    }
  };

  // Run closest-hit intersection on a buffer of rays
  // and pack object hit data into the ray
  class RayIntersectPrimitive : public detail::BaseInplaceBufferPrimitive {
    struct BufferLayout { uint n; };
    
    gl::Program             m_program;
    DispatchDividePrimitive m_prim_ddiv;
    gl::Buffer              m_buffer_count;
    BufferLayout           *m_buffer_count_map;
    
  public:
    // RayIntersectPrimitive(const Scene &scene);
    
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

  // Run any-hit intersection on a buffer of rays
  // and pack boolean hit data into the ray
  class RayIntersectAnyPrimitive : public detail::BaseInplaceBufferPrimitive {
    struct BufferLayout { uint n; };
    
    gl::Program             m_program;
    DispatchDividePrimitive m_prim_ddiv;
    gl::Buffer              m_buffer_count;
    BufferLayout           *m_buffer_count_map;

  public:
    // RayIntersectPrimitive(const Scene &scene);
    
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