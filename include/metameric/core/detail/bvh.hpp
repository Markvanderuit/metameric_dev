#pragma once

#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <array>
#include <bit>

namespace met {
  struct BVH {
    struct AABB {
      eig::AlArray3f minb, maxb;
    };

    struct Node {
      // AABBs of children; is not set for leaves
      std::array<AABB, 8> child_aabb;

      // Offset into child nodes or primitives, overlapped with flag bit
      // to indicate leaves
      uint offs_data, size_data;

    public:
      static constexpr uint32_t leaf_flag_bit = 1u << 31;
      inline constexpr bool is_leaf() const { return offs_data & leaf_flag_bit;    }
      inline constexpr uint    offs() const { return offs_data & (~leaf_flag_bit); }
      inline constexpr uint    size() const { return size_data;                    }
    };

    struct NodePack {
      uint aabb_pack_0;                 // lo.x, lo.y
      uint aabb_pack_1;                 // hi.x, hi.y
      uint aabb_pack_2;                 // lo.z, hi.z
      uint data_pack;                   // leaf | size | offs
      std::array<uint, 8> child_pack_0; // per child: lo.x | lo.y | hi.x | hi.y
      std::array<uint, 4> child_pack_1; // per child: lo.z | hi.z
    };
    static_assert(sizeof(NodePack) == 64);

  public:
    std::vector<Node> nodes; // Tree structure of inner nodes and leaves
    std::vector<uint> prims; // Unsorted indices of underlying primitivers
  };
  // BVH helper struct
  struct BVHCreateMeshInfo {
    const Mesh &mesh;                // Reference mesh to build BVH over
    uint n_node_children = 8;        // Maximum fan-out of BVH on each node
    uint n_leaf_children = 4;        // Maximum nr of primitives on each leaf
  };

  // BVH helper struct
  struct BVHCreateAABBInfo {
    std::span<const BVH::AABB> aabb; // Range of bounding boxes to build BVH over
    uint n_node_children = 8;        // Maximum fan-out of BVH on each node
    uint n_leaf_children = 4;        // Maximum nr of primitives on each leaf
  };

  BVH create_bvh(BVHCreateMeshInfo info);
  BVH create_bvh(BVHCreateAABBInfo info);
} // namespace met