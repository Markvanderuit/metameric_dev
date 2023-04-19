#include <metameric/core/data.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/dev/task_draw_bvh.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/pipeline/detail/bvh.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/utility.hpp>
#include <algorithm>
#include <bit>
#include <execution>
#include <ranges>
#include <vector>

namespace met {
  // Buffer flags for flushable, persistent, write-only mapping
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  // namespace detail {
  //   namespace radix {
  //     inline
  //     uint expand_bits_10(uint i)  {
  //       i = (i * 0x00010001u) & 0xFF0000FFu;
  //       i = (i * 0x00000101u) & 0x0F00F00Fu;
  //       i = (i * 0x00000011u) & 0xC30C30C3u;
  //       i = (i * 0x00000005u) & 0x49249249u;
  //       return i;
  //     }

  //     inline
  //     uint morton_code(eig::Vector3f v_) {
  //       auto v = (v_ * 1024.f).cwiseMax(0.f).cwiseMin(1023.f).eval();
  //       uint x = expand_bits_10(uint(v[0]));
  //       uint y = expand_bits_10(uint(v[1]));
  //       uint z = expand_bits_10(uint(v[2]));
  //       return x * 4u + y * 2u + z;
  //     }

  //     inline
  //     int find_msb(uint i) {
  //       return 31 - std::countl_zero(i);
  //     }

  //     // Find split position within a range [first, last] of morton codes
  //     inline
  //     uint find_split(std::span<const uint> codes, uint first, uint last) {
  //       // Initial guess for split position
  //       uint split = first;

  //       // Get data for initial guess 
  //       uint code = codes[first];
  //       uint pref = find_msb(code ^ codes[last]);

  //       // Perform a binary search to find the split position
  //       uint step = last - first;
  //       do {
  //         step = (step + 1) >> 1;        // Decrease step size
  //         uint new_split = split + step; // Possible new split position

  //         guard_continue(new_split < last);
  //         guard_continue(find_msb(code ^ codes[new_split]) < pref);

  //         split = new_split; // Accept newly proposed split for this iteration
  //       } while (step > 1);

  //       return split;
  //       /* return first + (last - first) / 2; // blunt halfway split for testing */
  //     }
  //   } // namespace radix

  //   /* Implicit tree helper code */

  //   // Generate a transforming view that performs unsigned integer index access over a range
  //   constexpr auto indexed_view(const auto &v) {
  //     return std::views::transform([&v](uint i) { return v[i]; });
  //   };

  //   // Hardcoded log2 of tree's degree
  //   template <uint Degr> consteval uint tree_degr_log();
  //   // template <> consteval uint tree_degr_log<2>() { return 1; }
  //   // template <> consteval uint tree_degr_log<4>() { return 2; }
  //   template <> consteval uint tree_degr_log<8>() { return 3; }

  //   // Beginning index of node on current tree level
  //   template <uint Degr> constexpr uint tree_lvl_begin(int lvl);
  //   // template <> constexpr uint tree_lvl_begin<2>(int lvl) { return 0xFFFFFFFF >> (32 - (lvl - 1)); }
  //   // template <> constexpr uint tree_lvl_begin<4>(int lvl) { return 0x55555555 >> (31 - ((tree_degr_log<4>() * lvl)) + 1); }
  //   template <> constexpr uint tree_lvl_begin<8>(int lvl) { return (0x92492492 >> (31 - tree_degr_log<8>() * lvl)) >> 3; }

  //   // Extent of indices on current tree level
  //   template <uint Degr> constexpr uint tree_lvl_extent(uint lvl) {
  //     return tree_lvl_begin<Degr>(lvl + 1) - tree_lvl_begin<Degr>(lvl);
  //   }

  //   // Last of indices on current tree level
  //   template <uint Degr> constexpr uint tree_lvl_end(uint lvl) {
  //     return tree_lvl_begin<Degr>(lvl + 1) - 1;
  //   }
    
  //   // Total nr. of nodes including padding, given nr. of tree levels
  //   template <uint Degr> constexpr uint tree_n_nodes(uint lvls) { 
  //     return tree_lvl_begin<Degr>(lvls + 1); 
  //   }

  //   // Total nr. of levels including padding, given nr. of leaf nodes
  //   template <uint Degr> constexpr uint tree_n_lvls(uint leaves) {
  //     return 1 + static_cast<uint>(std::ceil(std::log2(leaves) / tree_degr_log<Degr>()));
  //   }

  //   // Delaunay search tree; implict bvh structure
  //   template <uint Degr>
  //   struct ImplicitTree {
  //     constexpr static uint Degr  = Degr; // Maximum degree for non-leaf nodes
  //     constexpr static uint LDegr = tree_degr_log<Degr>(); // Useful constant

  //     struct Node {
  //       eig::Array3f b_min    = std::numeric_limits<float>::max(); // Bounding volume minimum
  //       uint         e_begin  = 0;                                 // Begin index of underlying element 
  //       eig::Array3f b_max    = std::numeric_limits<float>::min(); // Bounding volume center; volume max
  //       uint         e_extent = 0;                                 // Extent of underlying element range
  //     };
    
  //   public:
  //     // Default constructor
  //     ImplicitTree() = default;
      
  //     // Sized constructor
  //     ImplicitTree(uint n_objects)
  //     : n_objects(n_objects),
  //       n_levels(tree_n_lvls<Degr>(n_objects)),
  //       nodes(tree_n_nodes<Degr>(n_levels), Node { })
  //     { }

  //     // Public members
  //     uint n_levels;
  //     uint n_objects;
  //     std::vector<Node> nodes;
        
  //     // Node/leaf accessors/iterators
  //     size_t node_size_i(uint lvl)  const { return tree_lvl_extent<Degr>(lvl);                             }
  //     size_t node_begin_i(uint lvl) const { return tree_lvl_begin<Degr>(lvl);                              }
  //     size_t node_end_i(uint lvl)   const { return tree_lvl_begin<Degr>(lvl) + tree_lvl_extent<Degr>(lvl); }
  //     auto   node_begin(uint lvl)   const { return nodes.begin() + node_begin_i(lvl);                      }
  //     auto   node_end(uint lvl)     const { return nodes.begin() + node_end_i(lvl);                        }
  //     size_t leaf_begin_i()         const { return tree_lvl_begin<Degr>(n_levels - 1);                     }
  //     size_t leaf_end_i()           const { return tree_lvl_begin<Degr>(n_levels - 1) + n_objects;         }
  //     auto   leaf_begin()           const { return nodes.begin() + leaf_begin_i();                         }
  //     auto   leaf_end()             const { return nodes.begin() + leaf_end_i();                           }
  //   };

  //   template <typename Mesh = IndexedDelaunayData, uint Degr>
  //   std::pair<Mesh, ImplicitTree<Degr>> generate_search_tree_mod(Mesh delaunay) {
  //     met_trace();

  //     using Tree = ImplicitTree<Degr>;
  //     using Node = Tree::Node; 
      
  //     // Establish object centers as targets for morton order
  //     std::vector<eig::Vector3f> centers(delaunay.elems.size());
  //     std::transform(std::execution::par_unseq, range_iter(delaunay.elems), centers.begin(), [&](const eig::Array4u &el) {
  //       return ((delaunay.verts[el[0]] + delaunay.verts[el[1]] + delaunay.verts[el[2]] + delaunay.verts[el[3]]) / 4.f).eval();
  //     });

  //     // Build quick and dirty morton order
  //     std::vector<uint> codes(centers.size());
  //     std::vector<uint> order(centers.size());
  //     std::transform(std::execution::par_unseq, range_iter(centers), codes.begin(), radix::morton_code);
  //     std::iota(range_iter(order), 0u);
  //     std::sort(std::execution::par_unseq, range_iter(order), [&](uint i, uint j) { return codes[i] < codes[j]; });

  //     // Adjust mesh element data to adhere to sorted order
  //     auto _elems = delaunay.elems;
  //     std::transform(std::execution::par_unseq, range_iter(order), delaunay.elems.begin(), [&](uint i) { return _elems[i]; });

  //     // Initialize tree holder object; root node encompasses entire data range
  //     Tree tree(order.size());
  //     tree.nodes[0] = Node { .e_begin = 0, .e_extent = tree.n_objects };

  //     // Build subdivision based on morton order; push downwards
  //     for (int lvl = 0; lvl < tree.n_levels - 1; lvl++) {
  //       #pragma omp parallel for
  //       for (int i = tree.node_begin_i(lvl); i < tree.node_end_i(lvl); ++i) {
  //         // Load current node for subdivision
  //         const Node &node = tree.nodes[i];

  //         // Perform subdivision log2(Degr) times, starting at the current node; 
  //         // this adapts to binary/quad/octree configurations
  //         std::vector<Node> children = { node };
  //         for (int j = Tree::LDegr; j > 0; --j) { // 3, 2, 1
  //           std::vector<Node> _children;
  //           for (auto &node : children) {
  //             // Propagate leaf/empty nodes to bottom of tree
  //             if (node.e_extent <= 1) {
  //               _children.push_back(node);
  //               continue;
  //             }

  //             // Determine ranges of left/right nodes
  //             uint begin = node.e_begin, end = begin + node.e_extent - 1;
  //             uint split = radix::find_split(codes, begin, end);
              
  //             // Push subdivided nodes
  //             _children.push_back({ .e_begin = begin,     .e_extent = split - begin + 1 });
  //             _children.push_back({ .e_begin = split + 1, .e_extent = end - split });
  //           }
  //           children = _children;  
  //         } // for j

  //         // Store subdivided result in tree
  //         std::ranges::copy(children, tree.nodes.begin() + i * Tree::Degr + 1);
  //       } // for i
  //     } // for lvl

  //     // Build bounding volumes; pull upwards
  //     for (int lvl = tree.n_levels - 1; lvl >= 0; --lvl) {
  //       #pragma omp parallel for
  //       for (int i = tree.node_begin_i(lvl); i < tree.node_end_i(lvl); ++i) {
  //         // Load current node, skip padding nodes
  //         Node &node = tree.nodes[i];
  //         guard_continue(node.e_extent > 0);

  //         // Build data; separate into leaf/non-leaf cases 
  //         if (node.e_extent == 1 || lvl == tree.n_levels - 1) {  // Leaf node; 
  //           // Fit bounding volume around contained mesh vertices
  //           for (const auto &vt : std::views::iota(node.e_begin, node.e_begin + node.e_extent)
  //                               | indexed_view(delaunay.elems) | std::views::join | indexed_view(delaunay.verts)) {
  //             node.b_min = node.b_min.cwiseMin(vt);
  //             node.b_max = node.b_max.cwiseMax(vt);
  //           }
  //         } else {  // Non-leaf node; 
  //           // Generate bounding volume over non-empty children
  //           for (const auto &child : std::views::iota(1 + i * Tree::Degr, 1 + (i + 1) * Tree::Degr)
  //                                  | indexed_view(tree.nodes)) {
  //             node.b_min = node.b_min.cwiseMin(child.b_min);
  //             node.b_max = node.b_max.cwiseMax(child.b_max);
  //           }
  //         }
  //       } // end for i
  //     } // end for lvl

  //     return { delaunay, tree };
  //   }
  // } // namespace detail

  void ViewportDrawBVHTask::init(SchedulerHandle &info) {
    met_trace_full();
    
    // Setup mapped buffer objects
    m_unif_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_camr_buffer = {{ .size = sizeof(CameraBuffer), .flags = buffer_create_flags }};
    m_unif_map    = m_unif_buffer.map_as<UniformBuffer>(buffer_access_flags).data();
    m_camr_map    = m_camr_buffer.map_as<CameraBuffer>(buffer_access_flags).data();
    
    // Load shader object
    m_program = {{ .type = gl::ShaderType::eVertex,   
                   .spirv_path = "resources/shaders/views/dev/draw_bvh.vert.spv",
                   .cross_path = "resources/shaders/views/dev/draw_bvh.vert.json" },
                 { .type = gl::ShaderType::eFragment, 
                   .spirv_path = "resources/shaders/views/dev/draw_bvh.frag.spv",
                   .cross_path = "resources/shaders/views/dev/draw_bvh.frag.json" }};

    // Specify array and draw object
    m_array = {{ }};
    m_draw  = {
      .type             = gl::PrimitiveType::eTriangles,
      .vertex_count     = 0,
      .draw_op          = gl::DrawOp::eLine,
      .bindable_array   = &m_array,
      .bindable_program = &m_program
    };

    // Let's start at the top for now
    m_tree_level = 0;
  }
  
  void ViewportDrawBVHTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
    // Get external resources
    const auto &e_delaunay   = info("gen_convex_weights", "delaunay").read_only<AlignedDelaunayData>();
    const auto &e_view_state = info("state", "view_state").read_only<ViewportState>();

    // Build search tree and push results
    IndexedDelaunayData mesh_copy = convert_mesh<IndexedDelaunayData>(e_delaunay);
    detail::BVH<
      8,
      detail::BVHPrimitive::eTetrahedron
    > bvh(mesh_copy.verts, mesh_copy.elems);
    m_tree_buffer  = {{ .data = cast_span<std::byte>(bvh.nodes()) }};

    // auto [_, tree] = detail::generate_search_tree_mod<AlignedDelaunayData, 8>(e_delaunay);

    // Determine draw count
    auto node_level     = bvh.nodes(m_tree_level);
    uint draw_begin     = std::distance(bvh.nodes().begin(), node_level.begin() + m_tree_index); //  tree.node_begin_i(m_tree_level);
    uint draw_extent    = 1; // node_level.size();
    m_draw.vertex_count = 36 * draw_extent;

    // Push uniform data
    m_unif_map->node_begin  = draw_begin;
    m_unif_map->node_extent = draw_extent;
    m_unif_buffer.flush();

    // On relevant state change, update uniform buffer data
    if (e_view_state.camera_matrix || e_view_state.camera_aspect) {
      const auto &e_arcball = info("viewport.input", "arcball").read_only<detail::Arcball>();
      m_camr_map->matrix = e_arcball.full().matrix();
      m_camr_map->aspect = { 1.f, e_arcball.m_aspect };
      m_camr_buffer.flush();
    }

    // Set OpenGL state shared for the coming draw operations
    auto shared_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true),
                                 gl::state::ScopedSet(gl::DrawCapability::eBlendOp,   false),
                                 gl::state::ScopedSet(gl::DrawCapability::eCullOp,    false),
                                 gl::state::ScopedSet(gl::DrawCapability::eMSAA,      false) };

    // Bind resources and dispatch draw
    m_program.bind("b_tree", m_tree_buffer);
    m_program.bind("b_unif", m_unif_buffer);
    m_program.bind("b_camr", m_camr_buffer);

    gl::dispatch_draw(m_draw);

    // Spawn ImGui debug window
    if (ImGui::Begin("BVH debug window")) {
      uint tree_level_min = 0, tree_level_max = bvh.n_levels() - 1;
      ImGui::SliderScalar("Level", ImGuiDataType_U32, &m_tree_level, &tree_level_min, &tree_level_max);

      uint tree_index_min = 0, tree_index_max = bvh.nodes(m_tree_level).size() - 1;
      ImGui::SliderScalar("Index", ImGuiDataType_U32, &m_tree_index, &tree_index_min, &tree_index_max);

      const auto &node = bvh.nodes()[draw_begin];
      ImGui::Value("Node index", draw_begin);
      ImGui::Value("Node begin", node.i);
      ImGui::Value("Node end",   node.i + node.n - 1);
      ImGui::Value("Node size",  node.n);
    }
    ImGui::End();
  }
} // namespace met