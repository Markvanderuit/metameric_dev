// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
  
    std::string     m_program_key;
    InfoType        m_info;
    gl::ComputeInfo m_dispatch;
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