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

#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>

namespace met {
  // Scene and its internal components
  struct Scene;
  struct Emitter;
  struct Object;
  struct Settings;
  struct Uplifting;
  struct View;

  // Scene resources
  struct Image;
  template <uint K>
  struct BVH;
  template <typename Vt, typename El>
  struct MeshBase;
  template <typename Vt> 
  struct ConvexHullBase;
  
  // General mesh/delaunay/chull types used throughout the application
  using Mesh         = MeshBase<eig::Array3f,   eig::Array3u>;
  using AlMesh       = MeshBase<eig::AlArray3f, eig::Array3u>;
  using Delaunay     = MeshBase<eig::Array3f,   eig::Array4u>;
  using AlDelaunay   = MeshBase<eig::AlArray3f, eig::Array4u>;
  using ConvexHull   = ConvexHullBase<eig::Array3f>;
  using AlConvexHull = ConvexHullBase<eig::AlArray3f>;

  // Uplifting constraint vertices
  struct LinearConstraint;
  struct NLinearConstraint;
  struct MeasurementConstraint;
  struct DirectColorConstraint;
  struct DirectSurfaceConstraint;
  struct IndirectSurfaceConstraint;

  // Sampling distribution helpers
  class PCGEngine;
  template <typename E = PCGEngine> requires (std::uniform_random_bit_generator<E>)
  class UniformSampler;
  class Distribution;

  namespace detail {
    // Define maximum supported components for some types
    // These aren't up to device limits, but mostly exist so some 
    // sizes can be hardcoded shader-side in uniform buffers and
    // can be crammed into shared memory in some places
    constexpr static uint met_max_meshes      = MET_SUPPORTED_MESHES;
    constexpr static uint met_max_objects     = MET_SUPPORTED_OBJECTS;
    constexpr static uint met_max_emitters    = MET_SUPPORTED_EMITTERS;
    constexpr static uint met_max_constraints = MET_SUPPORTED_CONSTRAINTS;
    constexpr static uint met_max_textures    = MET_SUPPORTED_TEXTURES;
  } // namespace detail
} // namespace met