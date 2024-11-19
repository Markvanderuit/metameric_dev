#pragma once

#include <metameric/core/record.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/scene/scene.hpp>

namespace met::detail {
  // Given scene information obtained through e.g. ray hit data, build hit surface information
  SurfaceInfo query_surface_info(const Scene &scene, const eig::Array3f p, const SurfaceRecord &rc);

  // Given surface information, build uplifting tessellation data that the renderer would use
  // to uplift the surface diffuse color to a spectrum
  UpliftingInfo query_uplifting_info(const Scene &scene, const SurfaceInfo &si);

  // Repeat of query_uplifting_info applied to an entire queried path
  std::vector<UpliftingInfo> query_uplifting_info(const Scene &scene, const PathRecord &path);

  // Helper object for path reconstruction; a path's vertex reflectance is represented as
  // a x some constrained metamer + any remainder reflectances. This supports eq. 9 in the paper.
  struct ReconstructionInfo {
    float        a;         // Barycentric weight of reflectance at one of four vertices
    eig::Array4f remainder; // Remainder of 4-wavelength reflectances, premultiplied and summed
  };

  // Given a path record and a uplifting constraint, return the relevant reconstruction
  // info of uplifting along the path
  std::vector<ReconstructionInfo> query_path_reconstruction(const Scene &scene, const PathRecord &path, const ConstraintRecord &cs);

  // Convenient shorthands that take hit record data instead; which somehow
  // ended up being rather common
  inline SurfaceInfo query_surface_info(const Scene &scene, const VertexRecord &rc) {
    return query_surface_info(scene, rc.p, rc.record);
  }
  inline SurfaceInfo query_surface_info(const Scene &scene, const RayRecord &rc) {
    return query_surface_info(scene, rc.get_position(), rc.record);
  }
  inline UpliftingInfo query_uplifting_info(const Scene &scene, const VertexRecord &rc) {
    return query_uplifting_info(scene, query_surface_info(scene, rc));
  }
  inline UpliftingInfo query_uplifting_info(const Scene &scene, const RayRecord &rc) {
    return query_uplifting_info(scene, query_surface_info(scene, rc));
  }
  inline UpliftingInfo query_uplifting_info(const Scene &scene, const eig::Array3f p, const SurfaceRecord &rc) {
    return query_uplifting_info(scene, query_surface_info(scene, p, rc));
  }
} // namespace met::detail