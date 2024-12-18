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

#include <metameric/core/ranges.hpp>
#include <metameric/scene/detail/bvh.hpp>
#include <embree4/rtcore.h>
#include <algorithm>
#include <execution>
#include <memory>
#include <optional>
#include <stack>

namespace met::detail {
  struct BuildNode {
    virtual float sah()     const = 0;
    virtual bool  is_leaf() const = 0;
  };

  template <uint K>
  struct BuildNodeInner : public BuildNode {
    std::array<bool,        K> child_types;
    std::array<BuildNode *, K> child_nodes;
    std::array<AABB,        K> child_aabbs;
    
  public:
    BuildNodeInner() {
      child_types.fill(false);
      child_nodes.fill(nullptr);
    }

    float sah()     const override { return 0.f;   }
    bool  is_leaf() const override { return false; }
  };

  template <uint K>
  struct BuildNodeLeaf : public BuildNode {
    std::array<AABB, K>      child_aabbs;
    size_t                   n_prims;
    const RTCBuildPrimitive *prim_p;

  public:
    BuildNodeLeaf(const RTCBuildPrimitive *prim_p, size_t n_prims) 
    : prim_p(prim_p), 
      n_prims(n_prims) {
      for (size_t i = 0; i < n_prims; ++i)
        std::memcpy(&child_aabbs[i], &prim_p[i], sizeof(RTCBuildPrimitive));
    }

    float sah()     const override { return 1.f;  }
    bool  is_leaf() const override { return true; }
  };

  // Singleton rtc device; should be fine for now
  static std::optional<RTCDevice> rtc_device;

  void emb_error_callback(void *user_p, enum RTCError err, const char *str) {
    fmt::print("Embree caught an error\nExpand this!\nPanic!\n");
  }

  RTCDevice get_rtc_device() {
    if (!rtc_device) {
      rtc_device = rtcNewDevice(nullptr);
      rtcSetDeviceErrorFunction(rtc_device.value(), emb_error_callback, NULL);
    }
    return rtc_device.value();
  }

  template <uint K>
  void * bvh_init_leaf_node(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive *prims_p, size_t n_prims, void *user_p) {
    void* vptr = rtcThreadLocalAlloc(alloc, sizeof(BuildNodeLeaf<K>), 16);
    return (void *) new (vptr) BuildNodeLeaf<K>(prims_p, n_prims);
  }
  
  template <uint K>
  void * bvh_init_tree_node(RTCThreadLocalAllocator alloc, uint n_children, void *user_p) {
    void *ptr = rtcThreadLocalAlloc(alloc, sizeof(BuildNodeInner<K>), 16);
    return (void *) new (ptr) BuildNodeInner<K>;
  }
  
  template <uint K>
  void bvh_set_children(void *node_p, void **child_p, uint n_children, void *user_p) {
    auto node = static_cast<BuildNodeInner<K> *>(node_p);
    node->child_types.fill(false);
    for (size_t i = 0; i < n_children; ++i) {
      auto child = static_cast<BuildNode *>(child_p[i]);
      node->child_nodes[i] = child;
      node->child_types[i] = child->is_leaf();
    }
  }
  
  template <uint K>
  void bvh_set_bounds(void *node_p, const RTCBounds **bounds, uint n_children, void *user_p) {
    static_assert(sizeof(AABB) == sizeof(RTCBounds));
    
    auto node = static_cast<BuildNodeInner<K> *>(node_p);
    for (size_t i = 0; i < n_children; ++i) {
      std::memcpy(&(node->child_aabbs[i]), bounds[i], sizeof(RTCBounds));
    }
  }
  
  struct BVHCreateInternalInfo {
    std::span<const RTCBuildPrimitive> data; // Range of bounding boxes to build BVH over
    uint n_leaf_children;                    // Maximum nr of primitives in each leaf
  };
  
  template <uint K>
  BVH<K> create_bvh_internal(BVHCreateInternalInfo info) {
    met_trace();

    // Create modifiable copy of primitives; embree may re-order these freely
    // Note; we don't reserve further space for embree's high quality build
    std::vector<RTCBuildPrimitive> prims(range_iter(info.data));

    // Initializie new BVH structure
    RTCBVH rtc_bvh = rtcNewBVH(get_rtc_device());

    // Initialize new BVH arguments object and configure for a simple build
    RTCBuildArguments args = rtcDefaultBuildArguments();
    args.byteSize               = sizeof(args);
    args.bvh                    = rtc_bvh;
    args.buildFlags             = RTC_BUILD_FLAG_NONE;
    args.buildQuality           = RTCBuildQuality::RTC_BUILD_QUALITY_MEDIUM;
    args.maxDepth               = 1024;
    args.maxBranchingFactor     = K; // TODO read docs on these?!
    args.sahBlockSize           = 1; // TODO read docs on these?!
    args.minLeafSize            = info.n_leaf_children; // TODO read docs on these?!
    args.maxLeafSize            = info.n_leaf_children; // TODO read docs on these?!
    args.traversalCost          = 1.f;
    args.intersectionCost       = 1.f;
    args.createLeaf             = bvh_init_leaf_node<K>;
    args.createNode             = bvh_init_tree_node<K>;
    args.setNodeBounds          = bvh_set_bounds<K>;
    args.setNodeChildren        = bvh_set_children<K>;
    args.primitives             = prims.data();
    args.primitiveCount         = prims.size();
    args.primitiveArrayCapacity = prims.capacity();
    args.splitPrimitive         = nullptr;
    args.buildProgress          = nullptr;
    args.userPtr                = nullptr;
    
    // Construct BVH and acquire pointer to root node
    auto root_p = static_cast<BuildNode *>(rtcBuildBVH(&args));
        
    // Prepare external BVH format and resize its blocks
    BVH<K> bvh;
    bvh.nodes.reserve(prims.size() * 2 / K);
    bvh.prims.reserve(prims.size());

    // Do a BF-traversal across embree's BVH, and convert nodes/leaves to the external format
    std::deque<BuildNode *> work_queue;
    work_queue.push_back(root_p);
    while (!work_queue.empty()) {
      // Get next node
      auto next_p = work_queue.front();
      work_queue.pop_front();

      // Generate base node data; nodes/leaves overlap
      typename BVH<K>::Node node;

      // Dependent on node type, do...
      if (auto node_p = dynamic_cast<BuildNodeInner<K> *>(next_p)) {
        // Get coipy of non-nulled child data
        auto nodes = node_p->child_nodes
                   | vws::filter([](auto ptr) { return ptr != nullptr; })
                   | view_to<std::vector<BuildNode *>>();
        
        // Store AABBs of children, currently uncompressed
        node.child_aabb = node_p->child_aabbs;
        node.child_mask = node_p->child_types;

        // Store child range
        node.type   = false;
        node.offset = static_cast<uint>(bvh.nodes.size() + work_queue.size() + 1);
        node.size   = static_cast<uint>(nodes.size());

        // Push child pointers on back of queue for continued traversal
        rng::copy(nodes, std::back_inserter(work_queue));
      } else if (auto leaf_p = dynamic_cast<BuildNodeLeaf<K> *>(next_p)) {
        // Get copy of all contained primitive indices
        auto prims = std::span(leaf_p->prim_p, leaf_p->n_prims)
                   | vws::transform(&RTCBuildPrimitive::primID)
                   | view_to<std::vector<uint>>();
        
        // Store AABBs of children, currently uncompressed
        // Child data remains unspecified
        node.child_aabb = leaf_p->child_aabbs;
        node.child_mask.fill(true);

        // Store child range
        node.type   = true;
        node.offset = static_cast<uint>(bvh.prims.size());
        node.size   = static_cast<uint>(prims.size());
        
        // Store processed primitives in BVH
        rng::copy(prims, std::back_inserter(bvh.prims));
      }

      // Store processed node in BVH
      bvh.nodes.push_back(node);
    } // while (!work_queue.empty())
    
    // Release Embree's BVH data
    rtcReleaseBVH(rtc_bvh);

    return bvh;
  }

  template <uint K>
  BVH<K>::BVH(CreateMeshInfo info) {
    met_trace();

    // Build BVH primitive structs; use indexed iterator over mesh 
    // elements because we need to assign indices to them
    const auto &mesh = info.mesh;
    std::vector<RTCBuildPrimitive> prims(mesh.elems.size());
    #pragma omp parallel for
    for (int i = 0; i < mesh.elems.size(); ++i) {
      using VTy = Mesh::vert_type;
      using ETy = Mesh::elem_type;

      // Find the minimal/maximal components bounding the element
      auto vts  = mesh.elems[i] | vws::transform([&](uint i) { return mesh.verts[i]; }) | vws::take(3);
      auto minb = rng::fold_left_first(vts, [](VTy a, VTy b) { return a.cwiseMin(b).eval(); }).value();
      auto maxb = rng::fold_left_first(vts, [](VTy a, VTy b) { return a.cwiseMax(b).eval(); }).value();

      // Construct and assign BVH primitive object
      RTCBuildPrimitive prim;
      prim.geomID = 0;
      prim.primID = static_cast<uint>(i);
      std::tie(prim.lower_x, prim.lower_y, prim.lower_z) = { minb[0], minb[1], minb[2] };
      std::tie(prim.upper_x, prim.upper_y, prim.upper_z) = { maxb[0], maxb[1], maxb[2] };
      prims[i] = prim;
    } // for (int i)

    *this = create_bvh_internal<K>({ .data = prims, .n_leaf_children = info.n_leaf_children });
  }
  
  template <uint K>
  BVH<K>::BVH(CreateAABBInfo info) {
    met_trace();

    // Build BVH primitive structs; use indexed iterator over mesh 
    // elements because we need to assign indices to them
    std::vector<RTCBuildPrimitive> prims(info.aabb.size());
    #pragma omp parallel for
    for (int i = 0; i < info.aabb.size(); ++i) {
      auto aabb = info.aabb[i];
      RTCBuildPrimitive prim;
      prim.geomID = 0;
      prim.primID = static_cast<uint>(i);
      std::tie(prim.lower_x, prim.lower_y, prim.lower_z) = { aabb.minb[0], aabb.minb[1], aabb.minb[2] };
      std::tie(prim.upper_x, prim.upper_y, prim.upper_z) = { aabb.maxb[0], aabb.maxb[1], aabb.maxb[2] };
      prims[i] = prim;
    } // for (int i)

    *this = create_bvh_internal<K>({ .data = prims, .n_leaf_children = info.n_leaf_children });
  }

  /* Explicit template instantiations follow for supported BVH fanouts */

  template class BVH<2>;
  template class BVH<4>;
  template class BVH<8>;
} // namespace met::detail