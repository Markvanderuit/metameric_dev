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

#include <metameric/core/fwd.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <array>

namespace met::detail {
  // AABB representation used in cpu-side BVH
  struct AABB {
    eig::AlArray3f minb, maxb;

  public:
    // Abuse operator+ for reduce/fold
    AABB operator+(const AABB &o) {
      return { .minb = minb.cwiseMin(o.minb), .maxb = maxb.cwiseMax(o.maxb) };
    }
  };

  template <uint K>
  struct BVH {
    // BVH node representation; not for gpu-side packing, but
    // preparing for this step either way
    struct Node {
      // Underlying type; true == leaf, false  == node
      bool type;

      // Range of <= K underlying child nodes or primitives
      uint offset, size;

      // Child data, only set if type is leaf
      std::array<AABB, K> child_aabb; // AABB of <= K child node
      std::array<bool, K> child_mask; // Mask of <= K is_leaf/!is_leaf
    };

  public: // Internal BVH data
    std::vector<Node> nodes; // Tree structure of inner nodes and leaves
    std::vector<uint> prims; // Unsorted indices of underlying primitivers

  public: // Constructors
    // BVH helper struct; create BVH from mesh
    struct CreateMeshInfo {
      const Mesh &mesh;         // Reference mesh to build BVH over
      uint n_leaf_children = 4; // Maximum nr of primitives on each leaf
    };
    
    // BVH helper struct; create BVH from set of boxes
    struct CreateAABBInfo {
      std::span<const AABB> aabb; // Range of bounding boxes to build BVH over
      uint n_leaf_children = 4;   // Maximum nr of primitives on each leaf
    };

    BVH() = default;
    BVH(CreateMeshInfo info);
    BVH(CreateAABBInfo info);
  };
} // namespace met::detail