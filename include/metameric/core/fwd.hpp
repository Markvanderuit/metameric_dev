#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>

namespace met {
  // Major components
  struct Scene;

  // Scene components
  struct Emitter;
  struct Object;
  struct Settings;
  struct Uplifting;
  struct View;

  // Scene resources
  template <uint K>
  struct BVH;
  struct Image;
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
} // namespace met