#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/texture.hpp>

namespace met {
  class GenErrorMappingTask : public detail::TaskNode {
  public:
    using TextureType = gl::Texture2d4f;
    using TextureInfo = TextureType::InfoType;
  
  private:
    struct UniformBuffer {
      alignas(8) eig::Array2u size_in;  // Nr. of texels to sample from
      alignas(8) eig::Array2u size_out; // Nr. of texels to dispatch shader for
      alignas(4) uint         n_verts;  // Nr. of vertices defining meshing structure
      alignas(4) uint         n_elems;  // Nr. of elements defining meshing structure
    };

    bool              m_is_mutated;
    uint              m_mapping_i;
    TextureInfo       m_texture_info;

    gl::Buffer        m_unif_buffer;
    gl::Buffer        m_vert_buffer;
    gl::Program       m_program;
    gl::ComputeInfo   m_dispatch;

    UniformBuffer    *m_unif_map;
    std::span<AlColr> m_vert_map;

  public:
    GenErrorMappingTask(uint mapping_i);

    void init(SchedulerHandle &) override;
    bool is_active(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;

    void set_texture_info(SchedulerHandle &info, TextureInfo texture_info);
  };
} // namespace met