#include <metameric/core/data.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/dev/task_draw_bvh.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/utility.hpp>
#include <algorithm>
#include <execution>
#include <vector>

namespace met {
  // Buffer flags for flushable, persistent, write-only mapping
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  namespace detail {
    namespace radix {
      inline
      uint expand_bits_10(uint i)  {
        i = (i * 0x00010001u) & 0xFF0000FFu;
        i = (i * 0x00000101u) & 0x0F00F00Fu;
        i = (i * 0x00000011u) & 0xC30C30C3u;
        i = (i * 0x00000005u) & 0x49249249u;
        return i;
      }

      inline
      uint morton_code(eig::Vector3f v_) {
        auto v = (v_ * 1024.f).cwiseMax(0.f).cwiseMin(1023.f).eval();
        uint x = expand_bits_10(uint(v[0]));
        uint y = expand_bits_10(uint(v[1]));
        uint z = expand_bits_10(uint(v[2]));
        return x * 4u + y * 2u + z;
      }
    } // namespace radix

    /* Implicit tree helper code */

    // Hardcoded log2 of tree's degree
    template <uint Degr> consteval uint tree_degr_log();
    // template <> consteval uint tree_degr_log<2>() { return 1; }
    // template <> consteval uint tree_degr_log<4>() { return 2; }
    template <> consteval uint tree_degr_log<8>() { return 3; }

    // Beginning index of node on current tree level
    template <uint Degr> constexpr uint tree_lvl_begin(int lvl);
    // template <> constexpr uint tree_lvl_begin<2>(int lvl) { return 0xFFFFFFFF >> (32 - (lvl - 1)); }
    // template <> constexpr uint tree_lvl_begin<4>(int lvl) { return 0x55555555 >> (31 - ((tree_degr_log<4>() * lvl)) + 1); }
    template <> constexpr uint tree_lvl_begin<8>(int lvl) { return (0x92492492 >> (31 - tree_degr_log<8>() * lvl)) >> 3; }

    // Extent of indices on current tree level
    template <uint Degr> constexpr uint tree_lvl_extent(uint lvl) {
      return tree_lvl_begin<Degr>(lvl + 1) - tree_lvl_begin<Degr>(lvl);
    }
    
    // Total nr. of nodes including padding, given nr. of tree levels
    template <uint Degr> constexpr uint tree_n_nodes(uint lvls) { 
      return tree_lvl_begin<Degr>(lvls + 1); 
    }

    // Total nr. of levels including padding, given nr. of leaf nodes
    template <uint Degr> constexpr uint tree_n_lvls(uint leaves) {
      return 1 + static_cast<uint>(std::ceil(std::log2(leaves) / tree_degr_log<Degr>()));
    }

    // Delaunay search tree; implict bvh structure
    template <uint Degr>
    struct ImplicitTree {
      constexpr static uint Degree = Degr; // Maximum degree for non-leaf nodes
      
      struct Node {
        eig::Array3f b_min;    // Bounding volume minimum
        uint         e_begin;  // Begin index of underlying element 
        eig::Array3f b_max;    // Bounding volume center; volume max
        uint         e_extent; // Extent of underlying element range
      };
    
    public:
      // Default constructor
      ImplicitTree() = default;
      
      // Sized constructor
      ImplicitTree(uint n_objects)
      : n_objects(n_objects),
        n_levels(tree_n_lvls<Degr>(n_objects)),
        nodes(tree_n_nodes<Degr>(n_levels), Node { .e_extent = 0 })
      { }

      // Public members
      uint n_levels;
      uint n_objects;
      std::vector<Node> nodes;
        
      // Node/leaf accessors/iterators
      size_t node_size_i(uint lvl)  const { return tree_lvl_extent<Degr>(lvl);                             }
      size_t node_begin_i(uint lvl) const { return tree_lvl_begin<Degr>(lvl);                              }
      size_t node_end_i(uint lvl)   const { return tree_lvl_begin<Degr>(lvl) + tree_lvl_extent<Degr>(lvl); }
      auto   node_begin(uint lvl)   const { return nodes.begin() + node_begin_i(lvl);                      }
      auto   node_end(uint lvl)     const { return nodes.begin() + node_end_i(lvl);                        }
      size_t leaf_begin_i()         const { return tree_lvl_begin<Degr>(n_levels - 1);                     }
      size_t leaf_end_i()           const { return tree_lvl_begin<Degr>(n_levels - 1) + n_objects;         }
      auto   leaf_begin()           const { return nodes.begin() + leaf_begin_i();                         }
      auto   leaf_end()             const { return nodes.begin() + leaf_end_i();                           }
    };

    template <typename Mesh = IndexedDelaunayData, uint Degr>
    std::pair<Mesh, ImplicitTree<Degr>> generate_search_tree_mod(Mesh delaunay) {
      met_trace();

      using Tree = ImplicitTree<Degr>;
      using Node = Tree::Node; 
      
      // Establish object centers as targets for morton order
      std::vector<eig::Vector3f> centers(delaunay.elems.size());
      std::transform(std::execution::par_unseq, range_iter(delaunay.elems), centers.begin(), [&](const eig::Array4u &el) {
        return ((delaunay.verts[el[0]] + delaunay.verts[el[1]] + 
                 delaunay.verts[el[2]] + delaunay.verts[el[3]]) / 4.f).eval();
      });

      // Build quick and dirty morton order
      std::vector<uint> codes(centers.size());
      std::vector<uint> order(centers.size());
      std::transform(std::execution::par_unseq, range_iter(centers), codes.begin(), radix::morton_code);
      std::iota(range_iter(order), 0u);
      std::sort(std::execution::par_unseq, range_iter(order), [&](uint i, uint j) { return codes[i] < codes[j]; });

      // Adjust mesh element data to adhere to sorted order
      auto _elems = delaunay.elems;
      std::transform(std::execution::par_unseq, range_iter(order), delaunay.elems.begin(), [&](uint i) { return _elems[i]; });

      Tree tree(order.size());
      // fmt::print("n_children : {}\n", Tree::Degree);
      // fmt::print("levels     : {}\n", tree.n_levels);
      // fmt::print("n_objects  : {}\n", tree.n_objects);
      // fmt::print("n_nodes    : {}\n", tree.nodes.size());

      // Build bottom-most level
      #pragma omp parallel for
      for (int i = tree.leaf_begin_i(); i < tree.leaf_end_i(); ++i) { // extent of nr. of leaf objects, not nr. of leaf nodes
        uint i_underlying = i - static_cast<uint>(tree.leaf_begin_i());
      
        // Gather vertex data for the current element
        const eig::Array4u &el = delaunay.elems[i_underlying];
        std::array<eig::Array3f, 4> vt;
        std::ranges::transform(el, vt.begin(), [&](uint i) { return delaunay.verts[i]; });

        // Build node data, wrapping bbox around element
        tree.nodes[i] = Node { .b_min    = vt[0].min(vt[1].min(vt[2].min(vt[3]))).eval(),
                               .e_begin  = i_underlying,
                               .b_max    = vt[0].max(vt[1].max(vt[2].max(vt[3]))).eval(),
                               .e_extent = 1 };

        // fmt::print("{}\t{}\n", vt, tree.nodes[i].b_min);
      } // end for i
      
      // Build upper levels
      for (int lvl = tree.n_levels - 2; lvl >= 0; lvl--) {
        #pragma omp parallel for
        for (int i = tree.node_begin_i(lvl); i < tree.node_end_i(lvl); ++i) {
          // Start with new empty node as reduction target
          Node node = { .e_extent = 0 };

          uint j_begin = i * Tree::Degree + 1;
          for (uint j = j_begin; j < j_begin + Tree::Degree; ++j) {
            const Node &child = tree.nodes[j];

            // Test if either node contains data
            if (child.e_extent == 0) {
              continue;
            } else if (node.e_extent == 0) {
              node = child;
              continue;
            }
            
            // Reduce result into new node if both contain data
            node.e_begin  = std::min(node.e_begin, child.e_begin);
            node.e_extent = node.e_extent + child.e_extent; 
            node.b_min    = node.b_min.cwiseMin(child.b_min);
            node.b_max    = node.b_max.cwiseMax(child.b_max);
          } // end for j

          tree.nodes[i] = node;
        } // end for i
      } // end for lvl
      
      return { delaunay, tree };
    }
  } // namespace detail

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
    auto [_, tree] = detail::generate_search_tree_mod<AlignedDelaunayData, 8>(e_delaunay);
    m_tree_buffer  = {{ .data = cnt_span<std::byte>(tree.nodes) }};

    // Determine draw count
    uint draw_begin     = tree.node_begin_i(m_tree_level);
    uint draw_extent    = tree.node_size_i(m_tree_level);
    m_draw.vertex_count = 36 * draw_extent;

    fmt::print("{} - {}\n", draw_begin, draw_extent);

    // Push uniform data
    m_unif_map->node_begin  = draw_begin;
    m_unif_map->node_extent = draw_extent;
    m_unif_buffer.flush();

    static bool is_first_run = false;
    if (!is_first_run) {
      fmt::print("{} - {}\n", tree.nodes[0].b_min, tree.nodes[0].b_max);
      
      is_first_run = true;
      // std::exit(0);
    }

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
      uint pmin = 0, pmax = tree.n_levels - 1;
      ImGui::SliderScalar("Level", ImGuiDataType_U32, &m_tree_level, &pmin, &pmax);
    }
    ImGui::End();
  }
} // namespace met