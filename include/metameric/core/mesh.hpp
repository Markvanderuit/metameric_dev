#pragma once

#include <metameric/core/math.hpp>

namespace met {
  struct BasicAlMesh {
    using VertType = eig::AlArray3f;
    using ElemType = eig::Array3u;

    std::vector<VertType> vertices;
    std::vector<ElemType> elements; 
  };
} // namespace met