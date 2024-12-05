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

#include <metameric/scene/scene.hpp>
#include <metameric/editor/scene_viewport/task_combine.hpp>
#include <metameric/editor/detail/arcball.hpp>
#include <metameric/editor/detail/imgui.hpp>
#include <metameric/render/primitives_render.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/dispatch.hpp>

namespace met {
  bool ViewportCombineTask::is_active(SchedulerHandle &info) {
    return info.parent()("is_active").getr<bool>();
  }
  
  void ViewportCombineTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Initialize program object in cache
    std::tie(m_program_key, std::ignore) = info.global("cache").getw<gl::detail::ProgramCache>().set({{ 
      .type       = gl::ShaderType::eCompute,
      .glsl_path  = "shaders/editor/scene_viewport/combine.comp",
      .spirv_path = "shaders/editor/scene_viewport/combine.comp.spv",
      .cross_path = "shaders/editor/scene_viewport/combine.comp.json"
    }});

    // Initialize uniform buffers and corresponding mappings
    std::tie(m_unif_buffer, m_unif_buffer_map) = gl::Buffer::make_flusheable_object<UnifLayout>();
  }
    
  void ViewportCombineTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources 
    const auto &e_scene   = info.global("scene").getr<Scene>();
    const auto &e_target  = info.relative("viewport_image")("lrgb_target").getr<gl::Texture2d4f>();
    const auto &e_render  = info.relative("viewport_render")("renderer").getr<detail::IntegrationRenderPrimitive>();
    const auto &e_overlay = info.relative("viewport_draw_overlay")("target").getr<gl::Texture2d4f>();

    // Specify dispatch size
    auto dispatch_n    = e_target.size();
    auto dispatch_ndiv = ceil_div(dispatch_n, 16u);

    // Push miscellaneous uniforms
    m_unif_buffer_map->viewport_size       = dispatch_n;
    m_unif_buffer_map->sample_checkerboard = static_cast<uint>(e_render.is_pixel_checkerboard() && e_render.iter() <= 1);
    m_unif_buffer.flush();
    
    // Draw relevant program from cache
    auto &program = info.global("cache").getw<gl::detail::ProgramCache>().at(m_program_key);

    // Bind required resources to their corresponding targets
    program.bind("b_buff_unif",  m_unif_buffer);
    program.bind("b_render_4f",  e_render.film());
    program.bind("b_overlay_4f", e_overlay);
    program.bind("b_target_4f",  e_target);

    // Dispatch compute shader to add inputs to viewport target
    gl::sync::memory_barrier(gl::BarrierFlags::eImageAccess | gl::BarrierFlags::eTextureFetch);
    gl::dispatch_compute({ .groups_x         = dispatch_ndiv.x(),
                           .groups_y         = dispatch_ndiv.y(),
                           .bindable_program = &program       });
  }
} // namespace met