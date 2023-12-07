#include <metameric/core/detail/embree.hpp>
#include <metameric/core/utility.hpp>
#include <embree4/rtcore.h>
#include <algorithm>
#include <execution>
#include <memory>
#include <stack>

namespace met::detail {
  // Singleton rtc device; should be fine for now
  static std::optional<RTCDevice> rtc_device;

  void emb_error_callback(void *user_p, enum RTCError err, const char *str) {
    // ...
  }

  RTCDevice emb_device_init() {
    RTCDevice device = rtcNewDevice(nullptr);
    rtcSetDeviceErrorFunction(device, emb_error_callback, NULL);
    return device;
  }

  void emb_device_dstr(RTCDevice dev) {
    rtcReleaseDevice(dev);
  }

  RTCDevice &get_rtc_device() {
    if (!rtc_device)
      rtc_device = emb_device_init();
    return rtc_device.value();
  }

  RTCScene emb_init_scene(RTCDevice dev) {
    // Create new scene object
    RTCScene scene = rtcNewScene(dev);
    
    // Create new geometry object
    RTCGeometry geom = rtcNewGeometry(dev, RTC_GEOMETRY_TYPE_TRIANGLE);

    size_t n_tris = 3, n_elems = 1;
    size_t tri_size = 3 * sizeof(float), elem_size = 3 * sizeof(uint);

    auto verts_p = static_cast<eig::Array3f *>(rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, tri_size, n_tris));
    auto elems_p = static_cast<eig::Array3u *>(rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, elem_size, n_elems));
    auto verts = std::span(verts_p, n_tris);
    auto elems = std::span(elems_p, tri_size);

    // Set up with single triangle data 
    verts[0] = { 0, 0, 0 };
    verts[1] = { 1, 0, 0 };
    verts[2] = { 0, 1, 0 };
    elems[0] = { 0, 1, 2 };

    // Commit geometry to scene, attach to scene, then let it go
    rtcCommitGeometry(geom);
    rtcAttachGeometry(scene, geom);
    rtcReleaseGeometry(geom);

    // Commit scene data, starting BVH build
    rtcCommitScene(scene);

    // ...

    return scene;
  }

  BVHBBox merge(const BVHBBox &a, const BVHBBox &b) {
    return { .minb = a.minb.cwiseMin(b.minb), .maxb = a.maxb.cwiseMax(b.maxb) };
  }

  void emb_dstr_scene(RTCScene scene) {
    rtcReleaseScene(scene);
  }

  // Base struct
  struct RTCBVHNode {
    BVHBBox bounds;

    virtual bool is_inner() const = 0;
  };

  template <uint K>
  struct RTCBVHTreeNode : public RTCBVHNode {
    std::array<RTCBVHNode *, K> children;

    bool is_inner() const override { return true; }
  };

  struct RTCBVHLeafNode : public RTCBVHNode {
    RTCBVHLeafNode(const RTCBuildPrimitive *prim_p, size_t size)
    : prim_p(prim_p), size(size) { 
      if (size == 0) {
        bounds = { .minb = 0, .maxb = 0 };
        return;
      }

      std::memcpy(&bounds, prim_p, sizeof(RTCBuildPrimitive));
      for (size_t i = 1; i < size; ++i) {
        BVHBBox merge_bounds;
        std::memcpy(&merge_bounds, &prim_p[i], sizeof(RTCBounds));
        bounds = merge(bounds, merge_bounds);
      }
    }

    const RTCBuildPrimitive *prim_p;
    size_t size;

    bool is_inner() const override { return false; }
  };

  void * bvh_init_leaf_node(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive *prims_p, size_t n_prims, void *user_p) {
    void* vptr = rtcThreadLocalAlloc(alloc, sizeof(RTCBVHLeafNode), 16);
    return (void *) new (vptr) RTCBVHLeafNode(prims_p, n_prims);
  }

  template <uint K>
  void * bvh_init_tree_node(RTCThreadLocalAllocator alloc, uint n_children, void *user_p) {
    void *ptr = rtcThreadLocalAlloc(alloc, sizeof(RTCBVHTreeNode<K>), 16);
    return (void *) new (ptr) RTCBVHTreeNode<K>;
  }

  template <uint K>
  void bvh_set_children(void *node_p, void **child_p, uint n_children, void *user_p) {
    auto &node = *static_cast<RTCBVHTreeNode<K> *>(node_p);
    node.children.fill(nullptr);
    for (size_t i = 0; i < n_children; ++i)
      node.children[i] = static_cast<RTCBVHNode *>(child_p[i]);
  }

  template <uint K>
  void bvh_set_bounds(void *node_p, const RTCBounds **bounds, uint n_children, void *user_p) {
    auto &node = *static_cast<RTCBVHTreeNode<K> *>(node_p);
    std::memcpy(&node.bounds, bounds[0], sizeof(RTCBounds));
    for (size_t i = 1; i < n_children; ++i) {
      BVHBBox merge_bounds;
      std::memcpy(&merge_bounds, bounds[i], sizeof(RTCBounds));
      node.bounds = merge(node.bounds, merge_bounds);
    }
  }

  struct BVHCreateInternalInfo {
    std::span<const RTCBuildPrimitive> data; // Range of bounding boxes to build BVH over
    uint n_node_children;                    // Maximum fan-out of BVH on each node
    uint n_leaf_children;                    // Maximum nr of primitives on each leaf
  };

  template <uint K>
  BVH create_bvh_template(BVHCreateInternalInfo info) {
    met_trace();

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
    args.createLeaf             = bvh_init_leaf_node;
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
    auto root_p = static_cast<RTCBVHNode *>(rtcBuildBVH(&args));
    
    // Prepare external BVH format and resize its blocks
    BVH bvh;
    bvh.nodes.reserve(prims.size() * 2 / K);
    bvh.prims.reserve(prims.size() / 4);
    
    // Do a BF-traversal across embree's BVH, and convert nodes/leaves to the external format
    std::deque<RTCBVHNode *> work_queue;
    work_queue.push_back(root_p);
    while (!work_queue.empty()) {
      // Get next node
      auto next_p = work_queue.front();
      work_queue.pop_front();

      // Generate base node data; independent of leaf/inner presence for now
      BVH::Node node = { .minb = next_p->bounds.minb, .maxb = next_p->bounds.maxb };

      // Dependent on node type, do...
      if (auto node_p = dynamic_cast<RTCBVHTreeNode<K> *>(next_p)) {
        // Get view over all non-nulled children
        auto nodes = node_p->children
                   | vws::filter([](auto ptr) { return ptr != nullptr; })
                   | rng::to<std::vector>();
        
        // Set node offset/count to children in memory
        node.data0 = static_cast<uint>(bvh.nodes.size() + work_queue.size());
        node.data1 = static_cast<uint>(nodes.size());
        
        // Push child pointers on back of queue for continued traversal
        rng::copy(nodes, std::back_inserter(work_queue));
      } else if (auto leaf_p = dynamic_cast<RTCBVHLeafNode *>(next_p)) {
        // Get view over all contained primitive indices
        auto prims = std::span(leaf_p->prim_p, leaf_p->size)
                   | vws::transform(&RTCBuildPrimitive::primID);
        
        // Set leaf offset/count to primitives in memory;
        // and set flag bit to indicate that node is leaf
        node.data0 = static_cast<uint>(bvh.prims.size()) | BVH::Node::leaf_flag_bit;
        node.data1 = static_cast<uint>(prims.size()); 

        // Store processed primitives in BVH
        rng::copy(prims, std::back_inserter(bvh.prims));
      }

      // Store processed node in BVH
      bvh.nodes.push_back(node);
    } // while (!stack.empty())
    
    // Release Embree's BVH data
    rtcReleaseBVH(rtc_bvh);

    return bvh;
  }

  // Wrapper function to do templated generation with a runtime constant
  template <uint... Ks>
  BVH create_bvh_template_wrapper(BVHCreateInternalInfo info) {
    using FTy = BVH(*)(BVHCreateInternalInfo);
    constexpr FTy f[] = { create_bvh_template<Ks>... };
    return f[std::min(info.n_node_children, 8u) >> 2](info);
  }

  BVH create_bvh(BVHCreateMeshInfo info) {
    met_trace();

    // Build BVH primitive structs; use indexed iterator over mesh 
    // elements because we need to assign indices to them
    const auto &mesh = info.mesh;
    std::vector<RTCBuildPrimitive> prims(mesh.elems.size());
    #pragma omp parallel for
    for (int i = 0; i < mesh.elems.size(); ++i) {
      using VTy = Mesh::vert_type;
      using ETy = Mesh::elem_type;

      const ETy el = mesh.elems[i];
      
      // Find the minimal/maximal components bounding the element
      auto vts = el | vws::transform([&](uint i) { return mesh.verts[i]; }) | vws::take(3);
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

    return create_bvh_template_wrapper<2, 4, 8>({
      .data = prims, .n_node_children = info.n_node_children, .n_leaf_children = info.n_leaf_children
    });
  }
  
  BVH create_bvh(BVHCreateBBoxInfo info) {
    met_trace();

    // Build BVH primitive structs; use indexed iterator over mesh 
    // elements because we need to assign indices to them
    std::vector<RTCBuildPrimitive> prims(info.bbox.size());
    #pragma omp parallel for
    for (int i = 0; i < info.bbox.size(); ++i) {
      auto bbox = info.bbox[i];
      RTCBuildPrimitive prim;
      prim.geomID = 0;
      prim.primID = static_cast<uint>(i);
      std::tie(prim.lower_x, prim.lower_y, prim.lower_z) = { bbox.minb[0], bbox.minb[1], bbox.minb[2] };
      std::tie(prim.upper_x, prim.upper_y, prim.upper_z) = { bbox.maxb[0], bbox.maxb[1], bbox.maxb[2] };
      prims[i] = prim;
    } // for (int i)

    return create_bvh_template_wrapper<2, 4, 8>({
      .data = prims, .n_node_children = info.n_node_children, .n_leaf_children = info.n_leaf_children
    });
  }
} // met::detail