#pragma once

#include <metameric/core/mesh.hpp>
#include <metameric/core/packing.hpp>
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

  // TODO remove
  /* inline
  BVH::NodePack bvhpack(const BVH::Node &node) {
    // Obtain a merger of the child bounding boxes
    constexpr auto merge = [](const BVH::AABB &a, const BVH::AABB &b) -> BVH::AABB {
      return { .minb = a.minb.cwiseMin(b.minb), .maxb = a.maxb.cwiseMax(b.maxb) };
    };
    auto aabb = rng::fold_left_first(node.child_aabb.begin(), 
                                     node.child_aabb.begin() + node.size(), merge).value();
    // static uint bvh_print_i = 0;
    // fmt::print("{} - {}, minb = {}, maxb = {}, offs = {}, size = {}\n", 
    //   bvh_print_i++, node.is_leaf() ? "leaf" : "node", aabb.minb, aabb.maxb, node.offs(), node.size());

    BVH::NodePack p;

    // 3xu32 packs AABB lo, ex
    auto b_lo_in = aabb.minb;
    auto b_ex_in = (aabb.maxb - aabb.minb).eval();
    p.aabb_pack_0 = pack_unorm_2x16_floor({ b_lo_in.x(), b_lo_in.y() });
    p.aabb_pack_1 = pack_unorm_2x16_ceil ({ b_ex_in.x(), b_ex_in.y() });
    p.aabb_pack_2 = pack_unorm_2x16_floor({ b_lo_in.z(), 0 }) 
                  | pack_unorm_2x16_ceil ({ 0, b_ex_in.z() });

    // 1xu32 packs node type, child offset, child count
    p.data_pack = node.offs_data | (node.size_data << 27);

    // Child AABBs are packed in 6 bytes per child
    p.child_pack_0.fill(0);
    p.child_pack_1.fill(0);
    for (uint i = 0; i < node.size(); ++i) {
      auto b_lo_safe = ((node.child_aabb[i].minb - b_lo_in) / b_ex_in).eval();
      auto b_hi_safe = ((node.child_aabb[i].maxb - b_lo_in) / b_ex_in).eval();
      auto pack_0 = pack_unorm_4x8_floor((eig::Array4f() << b_lo_safe.head<2>(), 0, 0).finished())
                  | pack_unorm_4x8_ceil ((eig::Array4f() << 0, 0, b_hi_safe.head<2>()).finished());
      auto pack_1 = pack_unorm_4x8_floor((eig::Array4f() << b_lo_safe.z(), 0, 0, 0).finished())
                  | pack_unorm_4x8_ceil ((eig::Array4f() << 0, b_hi_safe.z(), 0, 0).finished());
      p.child_pack_0[i    ] |= pack_0;
      p.child_pack_1[i / 2] |= (pack_1 << ((i % 2) ? 16 : 0));
    }

    return p;
  } */

  /* inline
  BVH::Node unpack(const BVH::NodePack &p) {
    BVH::Node n;

    // Recover size, offset and node type data
    n.size_data = (p.data_pack >> 27) & 0xF;
    n.offs_data = (p.data_pack & (~(0xF << 27)));

    // Recover AABB lo, ex
    auto b_lo_out = (eig::Array3f() << unpack_unorm_2x16(p.aabb_pack_0), unpack_unorm_2x16(p.aabb_pack_2).x()).finished();
    auto b_ex_out = (eig::Array3f() << unpack_unorm_2x16(p.aabb_pack_1), unpack_unorm_2x16(p.aabb_pack_2).y()).finished();

    // Recover child AABB lo, hi
    for (uint i = 0; i < n.size(); ++i) {
      auto unpack_0 = unpack_unorm_4x8(p.child_pack_0[i    ]);
      auto unpack_1 = unpack_unorm_4x8(p.child_pack_1[i / 2] >> ((i % 2) ? 16 : 0));

      auto b_child_lo_safe = (eig::Array3f() << unpack_0.head<2>(), unpack_1.x()).finished();
      auto b_child_hi_safe = (eig::Array3f() << unpack_0.tail<2>(), unpack_1.y()).finished();

      n.child_aabb[i] = { .minb = (b_child_lo_safe * b_ex_out + b_lo_out),
                          .maxb = (b_child_hi_safe * b_ex_out + b_lo_out) };
    }

    return n;
  } */

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