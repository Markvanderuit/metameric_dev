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
#include <metameric/scene/scene.hpp>
#include <metameric/render/detail/primitives.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/sampler.hpp>

namespace met {
  // Helper struct for creation of PathRenderPrimitive
  struct PathRenderPrimitiveInfo {
    // Number of samples per pixel when a renderer primitive is invoked
    uint spp_per_iter = 1;

    // Renderer primitives will accumulate up to this number. Afterwards
    // the target is left unmodified. If set to 0, no limit is imposed.
    uint spp_max = std::numeric_limits<uint>::max();

    // Maximum path length (unused if 0) and russian roulette start (unused if set to 0)
    uint max_depth = 0;
    uint rr_depth  = PathRecord::path_max_depth;

    // Render pixels each other frame, alternating between checkerboards
    bool pixel_checkerboard = false;

    // Render output to image with an alpha component,
    // allowing images without a background
    bool enable_alpha = false;

    // Query a value (e.g. albedo), integrate it, and return
    bool enable_debug = false;

    // Program cache; enforced given the shader's long compile time
    ResourceHandle cache_handle;
  };
  	
  // Rendering primitive; implementation of a unidirectional spectral path
  // tracer with next-event-estimation and four-wavelength sampling.
  class PathRenderPrimitive : public detail::IntegrationRenderPrimitive {
    // Handle to program cache, and key for relevant program
    ResourceHandle  m_cache_handle;
    std::string     m_cache_key; 

    // Internal GL objects
    gl::ComputeInfo m_dispatch;
    gl::Sampler     m_sampler; // linear sampler

  public:
    using InfoType = PathRenderPrimitiveInfo;
    
    PathRenderPrimitive() = default;
    PathRenderPrimitive(InfoType info);

    void reset(const Sensor &sensor, const Scene &scene) override;
    const gl::Texture2d4f &render(const Sensor &sensor, const Scene &scene) override;
  };
} // namespace met