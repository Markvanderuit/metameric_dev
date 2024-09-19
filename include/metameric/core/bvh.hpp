#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <array>

namespace met {
  struct BVH {
    // AABB representation used in cpu-side BVH
    struct AABB {
      eig::AlArray3f minb, maxb;
    };

    // BVH node representation; not for gpu-side packing
    struct Node {
      // AABBs of children; is not set for leaves
      std::array<AABB, 8> child_aabb;

      // Offset into child nodes or primitives, overlapped with flag bit
      // to indicate leaves
      uint offs_data, size_data;

    public:
      inline constexpr bool is_leaf() const { return offs_data & 0x80000000u;    }
      inline constexpr uint    offs() const { return offs_data & (~0x80000000u); }
      inline constexpr uint    size() const { return size_data;                  }
    };

  public: // Internal BVH data
    std::vector<Node> nodes; // Tree structure of inner nodes and leaves
    std::vector<uint> prims; // Unsorted indices of underlying primitivers

  public: // Constructors
    // BVH helper struct; create BVH from mesh
    struct CreateMeshInfo {
      const Mesh &mesh;                // Reference mesh to build BVH over
      uint n_node_children = 8;        // Maximum fan-out of BVH on each node
      uint n_leaf_children = 4;        // Maximum nr of primitives on each leaf
    };

    // BVH helper struct; create BVH from set of boxes
    struct CreateAABBInfo {
      std::span<const BVH::AABB> aabb; // Range of bounding boxes to build BVH over
      uint n_node_children = 8;        // Maximum fan-out of BVH on each node
      uint n_leaf_children = 4;        // Maximum nr of primitives on each leaf
    };

    BVH() = default;
    BVH(CreateMeshInfo info);
    BVH(CreateAABBInfo info);
  };
} // namespace met