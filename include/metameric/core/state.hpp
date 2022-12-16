#pragma once

#include <metameric/core/utility.hpp>
#include <vector>

namespace met {
  /* Wrapper object for tracking changes to project data in program pipeline */
  struct ProjectState {
    using CacheStale = bool;

    struct CacheVert {
      CacheStale any;
      CacheStale any_colr_j;
      CacheStale any_mapp_j;

      CacheStale colr_i;
      CacheStale mapp_i;

      std::vector<CacheStale> colr_j;
      std::vector<CacheStale> mapp_j;
    };

  public:
    CacheStale any;
    CacheStale any_verts;
    CacheStale any_elems;
    CacheStale any_mapps;

    std::vector<CacheVert> verts;
    std::vector<CacheStale> elems;
    std::vector<CacheStale> mapps;
  };
} // namespace 