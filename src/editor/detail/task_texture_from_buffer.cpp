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

#include <metameric/editor/detail/task_texture_from_buffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met::detail {
  template <typename Ty>
  void TextureFromBufferTask<Ty>::init(SchedulerHandle &info) {
    met_trace_full();

    // Emplace texture resource using provided info object
    info(m_info.output_key).template init<Ty, typename Ty::InfoType>(m_info.texture_info);
    
    // Compute nr. of workgroups as nearest upper divide of n / (16, 16), implying wg size of 256
    eig::Array2u dispatch_n    = m_info.texture_info.size;
    eig::Array2u dispatch_ndiv = ceil_div(dispatch_n, 16u);

    // Instantiate uniform buffer block with provided settings
    struct UniformBuffer {
      alignas(8) eig::Array2u size;
      alignas(4) uint lrgb_to_srgb;
    } uniform_data {
      .size         = dispatch_n, 
      .lrgb_to_srgb = m_info.lrgb_to_srgb 
    };

    // Initialize objects for texture-to-texture resampling
    m_program = {{ .type        = gl::ShaderType::eCompute,
                    .glsl_path  = "shaders/editor/detail/buffer_to_texture_rgba32f.comp",
                    .spirv_path = "shaders/editor/detail/buffer_to_texture_rgba32f.comp.spv",
                    .cross_path = "shaders/editor/detail/buffer_to_texture_rgba32f.comp.json" }};
    m_dispatch = { .groups_x = dispatch_ndiv.x(),
                   .groups_y = dispatch_ndiv.y(),
                   .bindable_program = &m_program };
    m_uniform = {{ .data = obj_span<const std::byte>(uniform_data) }};
  }

  template <typename Ty>
  bool TextureFromBufferTask<Ty>::is_active(SchedulerHandle &info) {
    met_trace();
    auto rsrc = info(m_info.input_key.first, m_info.input_key.second);
    return rsrc.is_init() && rsrc.is_mutated();
  }

  template <typename Ty>
  void TextureFromBufferTask<Ty>::eval(SchedulerHandle &info) {
    met_trace_full();
    m_program.bind("b_uniform", m_uniform);
    m_program.bind("b_buffer",  info(m_info.input_key.first, m_info.input_key.second).template getr<gl::Buffer>());
    m_program.bind("i_image",   info(m_info.output_key).template getw<Ty>());
    gl::dispatch_compute(m_dispatch);
  }

  /* Explicit template instantiations for TextureFromBufferTask<> */

  template class TextureFromBufferTask<gl::Texture2d3f>;
  template class TextureFromBufferTask<gl::Texture2d4f>;
} // namespace met::detail