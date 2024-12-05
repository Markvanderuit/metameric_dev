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

#include <metameric/scene/scene.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/render/detail/primitives.hpp>
#include <small_gl/sampler.hpp>

namespace met {
  // Helper object for creation of PathQueryPrimitive and PartialPathQueryPrimitive
  struct PathQueryPrimitiveInfo {
    // Maximum path length
    uint max_depth = PathRecord::path_max_depth;
    
    // Program cache; enforced given the shader's long compile time
    ResourceHandle cache_handle;
  };

  // Helper object for creation of RayQueryPrimitive
  struct RayQueryPrimitiveInfo {
    // Program cache; enforced given the shader's long compile time
    ResourceHandle cache_handle;
  };
  
  // Primitive to query light transport along a single ray and get information
  // on each path
  class PathQueryPrimitive : public detail::BaseQueryPrimitive {
    ResourceHandle m_cache_handle;
    std::string    m_cache_key; 
    uint           m_max_depth;

    // Output data mappings and sync objects
    uint                   *m_output_head_map;
    std::span<PathRecord>   m_output_data_map;
    mutable gl::sync::Fence m_output_sync;

    // Internal GL objects
    gl::Sampler m_sampler; // linear sampler
    
    // Buffer storing CDF for wavelength sampling at path start
    Spec m_wavelength_distr;
    gl::Buffer m_wavelength_distr_buffer;

  public:
    using InfoType = PathQueryPrimitiveInfo;

    PathQueryPrimitive() = default;
    PathQueryPrimitive(InfoType info);
    
    // Take n samples and return output buffer
    const gl::Buffer &query(const PixelSensor &sensor, const Scene &scene, uint spp);

    // Wait for sync object, and then return output data
    std::span<const PathRecord> data() const;
  };

  // Primitive to perform a simple raycast
  class RayQueryPrimitive : public detail::BaseQueryPrimitive {
    // Handle to program cache, and key for relevant program
    ResourceHandle  m_cache_handle;
    std::string     m_cache_key; 

    // Output data mappings and sync objects
    RayRecord              *m_output_map;
    mutable gl::sync::Fence m_output_sync;

  public:
    using InfoType = RayQueryPrimitiveInfo;

    RayQueryPrimitive() = default;
    RayQueryPrimitive(InfoType info);

    // Take 1 sample and return output buffer
    const gl::Buffer &query(const RaySensor &sensor, const Scene &scene);

    // Wait for sync object, and then return output data
    const RayRecord &data() const;
  };
} // namespace met