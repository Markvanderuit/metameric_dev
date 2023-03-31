#pragma once

#include <metameric/core/math.hpp>
#include <vector>

namespace met {
  // Helper object for tracking data mutation across a vector structure
  template <typename CacheObject>
  struct VectorState {
    std::vector<CacheObject> is_stale;

  public:  
    constexpr
    const CacheObject& operator[](uint i) const { return is_stale[i]; }
    constexpr
          CacheObject& operator[](uint i)       { return is_stale[i]; }
    constexpr 
    auto size() const { return is_stale.size(); }

  public:
    bool is_any_stale;

    constexpr
    operator bool() const { return is_any_stale; }
  };

  // Helper object for tracking data mutation in ProjectData::Vert structure
  struct ProjectVertState {
    bool              colr_i;
    bool              csys_i;
    VectorState<uint> colr_j;
    VectorState<uint> csys_j;

  public:  
    bool is_any_stale;

    constexpr
    operator bool() const { return is_any_stale; }
  };

  // Helper object for tracking data mutation in ProjectData object
  struct ProjectState {
  public:  
    VectorState<ProjectVertState> verts;
    VectorState<uint>             elems;
    VectorState<uint>             csys;
    VectorState<uint>             cmfs;
    VectorState<uint>             illuminants;

  public:  
    bool is_any_stale;

    constexpr
    operator bool() const { return is_any_stale; }
  };

  // Helper object for tracking data mutation throughout viewport pipeline, e.g. vertex selection and camera movement
  struct ViewportState {
  public:
    // Camera properties
    bool camera_matrix;
    bool camera_aspect;

    // Vertex selection in viewport
    bool vert_selection;
    bool vert_mouseover;

    // Constraint selection in viewport overlay
    bool cstr_selection;
  
  public:  
    bool is_any_stale;

    constexpr
    operator bool() const { return is_any_stale; }
  };
} // namespace met