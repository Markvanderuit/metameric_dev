#pragma once

#include "metameric/core/mesh.hpp"
#include <array>

namespace met::detail {
  // Basic BVH axis-aligned bounding box
  // Fit for std140/430 layout, and matches embree's RTCBounds alignment
  struct BVHBBox {
    eig::AlArray3f minb, maxb;
  };

  // BVH helper struct
  struct BVHCreateMeshInfo {
    const Mesh &mesh;               // Reference mesh to build BVH over
    uint n_node_children = 2;       // Maximum fan-out of BVH on each node
    uint n_leaf_children = 1;       // Maximum nr of primitives on each leaf
  };

  struct BVHCreateBBoxInfo {
    std::span<const BVHBBox> bbox;  // Range of bounding boxes to build BVH over
    uint n_node_children = 2;       // Maximum fan-out of BVH on each node
    uint n_leaf_children = 1;       // Maximum nr of primitives on each leaf
  };

  // Generic BVH over a structure of bbox primitives;
  // Does not link back to mesh data directly, so mesh
  // needs to be queried for triangle data separately
  struct BVH {
    // Generic packed node type; fits both inner node and leaf
    struct Node {
      eig::Array3f minb;  // Minima of axis-aligned bounding box
      uint         data0; // Offset into child nodes or prims, overlapped with flag bit
      eig::Array3f maxb;  // Maxima of axis-aligned bounding box
      uint         data1; // Count of child nodes or prims

      // A flag bit used to distinguish inner nodes and leaves
      static constexpr uint32_t leaf_flag_bit = 1u << 31;
      inline constexpr bool is_leaf() const { return (data0 & leaf_flag_bit); }

      // Accessor helpers
      inline constexpr uint prim_offs() const { return data0 & (~leaf_flag_bit); }
      inline constexpr uint node_offs() const { return data0; }
      inline constexpr uint prim_size() const { return data1; }
      inline constexpr uint node_size() const { return data1; }
    };
    static_assert(sizeof(Node) == 32);

    std::vector<Node> nodes; // Tree structure of inner nodes and leaves
    std::vector<uint> prims; // Unsorted indices of underlying primitivers
  };

  BVH create_bvh(BVHCreateMeshInfo info);
  BVH create_bvh(BVHCreateBBoxInfo info);
} // met::detail