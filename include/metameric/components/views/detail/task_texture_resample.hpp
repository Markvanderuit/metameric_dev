#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/trace.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/sampler.hpp>

namespace met::detail {
  template <class TextureType>
  struct TextureResampleTaskInfo {
    using StringPair  = std::pair<std::string, std::string>;
    using TextureInfo = TextureType::InfoType;
    using SamplerInfo = gl::Sampler::InfoType;

    StringPair  input_key;             // Key to input resource
    std::string output_key;            // Key to output resource
    TextureInfo texture_info  = {};    // Info about output gl texture object
    SamplerInfo sampler_info  = {};    // Info about internal gl sampler object
    bool        lrgb_to_srgb  = false; // Perform gamma correction during resampling
  };

  template <class TextureTy>
  class TextureResampleTask : public detail::TaskNode {
    using TextureType = TextureTy;
    using InfoType    = TextureResampleTaskInfo<TextureType>;

    struct UniformBuffer {
      alignas(8) eig::Array2u size;
      alignas(4) uint lrgb_to_srgb;
    };
  
    InfoType        m_info;
    gl::ComputeInfo m_dispatch;
    gl::Program     m_program;
    gl::Sampler     m_sampler;
    gl::Buffer      m_uniform_buffer;
    UniformBuffer  *m_uniform_map;
    bool            m_is_mutated;

  public:
    TextureResampleTask(InfoType info)
    : m_info(info),
      m_is_mutated(false) { }
                        
    void init(SchedulerHandle &info) override;
    bool is_active(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;

    void set_texture_info(SchedulerHandle &info, TextureType::InfoType texture_info);
    void set_sampler_info(SchedulerHandle &info, gl::Sampler::InfoType sampler_info);
  };
} // namespace met::detail