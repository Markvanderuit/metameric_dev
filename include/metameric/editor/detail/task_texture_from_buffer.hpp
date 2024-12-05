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
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met::detail {
  template <class TextureType>
  struct TextureFromBufferTaskInfo {
    using StringPair  = std::pair<std::string, std::string>;
    using TextureInfo = TextureType::InfoType;

    StringPair  input_key;             // Key to input resource
    std::string output_key;            // Key to output resource
    TextureInfo texture_info = {};     // Info about output gl texture object
    bool        lrgb_to_srgb  = false; // Perform gamma correction during conversion
  };

  template <class TextureType>
  class TextureFromBufferTask : public detail::TaskNode {
    using InfoType = TextureFromBufferTaskInfo<TextureType>;

    InfoType        m_info;
    gl::ComputeInfo m_dispatch;
    gl::Program     m_program;
    gl::Buffer      m_uniform;

  public:
    TextureFromBufferTask(InfoType info)
    : m_info(info) { }
    
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
    bool is_active(SchedulerHandle &) override;
  };
} // namespace met::detail