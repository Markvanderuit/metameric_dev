#pragma once

#include <vector>

namespace met {
  /* Wrapper object for tracking changes to project data throughout program pipeline */
  struct ProjectState {
    struct CacheVert {
      bool any;
      bool any_colr_j;
      bool any_mapp_j;

      bool colr_i;
      bool mapp_i;

      std::vector<bool> colr_j;
      std::vector<bool> mapp_j;
    };

  public:
    bool any;
    bool any_verts;
    bool any_elems;
    bool any_mapps;

    std::vector<CacheVert> verts;
    std::vector<bool> elems;
    std::vector<bool> mapps;
  };

  /* Wrapper objecct for tracking changes to viewport in program pipeline; e.g. vertex selection */
  struct ViewportState {
    // Vertex selection in viewport
    bool vert_selection;
    bool vert_mouseover;

    // Element selection in viewport
    bool elem_selection;
    bool elem_mouseover;

    // Constraint selection in viewport overlay
    bool cstr_selection;
  };
} // namespace met