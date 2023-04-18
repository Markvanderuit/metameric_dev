#include <metameric/core/data.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/components/pipeline/task_gen_delaunay_weights.hpp>
#include <small_gl/utility.hpp>
#include <algorithm>
#include <execution>
#include <ranges>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
  constexpr uint buffer_init_size    = 512u;

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
      constexpr static uint Degr = Degr; // Maximum degree for non-leaf nodes
      
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
    std::pair<Mesh, ImplicitTree<Degr>> generate_search_tree(Mesh delaunay) {
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
      // fmt::print("n_children : {}\n", Tree::Degr);
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

          uint j_begin = i * Tree::Degr + 1;
          for (uint j = j_begin; j < j_begin + Tree::Degr; ++j) {
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

  void GenDelaunayWeightsTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_colr_data = e_appl_data.loaded_texture;
    const auto &e_proj_data = e_appl_data.project_data;
    
    // Determine compute dispatch size
    const uint dispatch_n    = e_colr_data.size().prod();
    const uint dispatch_ndiv = ceil_div(dispatch_n, 256u);

    // Initialize objects for compute dispatch
    m_program = {{ .type       = gl::ShaderType::eCompute,
                   .spirv_path = "resources/shaders/pipeline/gen_delaunay_weights.comp.spv",
                   .cross_path = "resources/shaders/pipeline/gen_delaunay_weights.comp.json" }};
    m_dispatch = { .groups_x = dispatch_ndiv, 
                   .bindable_program = &m_program }; 

    // Initialize uniform buffer and writeable, flushable mapping
    m_uniform_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_uniform_map    = &m_uniform_buffer.map_as<UniformBuffer>(buffer_access_flags)[0];
    m_uniform_map->n = e_appl_data.loaded_texture.size().prod();

    // Initialize buffer data
    gl::Buffer colr_buffer = {{ .data = cast_span<const std::byte>(io::as_aligned((e_colr_data)).data()) }};
    gl::Buffer vert_buffer = {{ .size = buffer_init_size * sizeof(eig::Array4f), .flags = buffer_create_flags}};
    gl::Buffer elem_buffer = {{ .size = buffer_init_size * sizeof(eig::Array4u), .flags = buffer_create_flags}};

    // Initialize writeable, flushable mappings over relevant buffers
    m_vert_map = vert_buffer.map_as<eig::AlArray3f>(buffer_access_flags);
    m_elem_map = elem_buffer.map_as<eig::Array4u>(buffer_access_flags);

    // Initialize buffer holding barycentric weights
    info("delaunay").set<AlignedDelaunayData>({ });  // Generated delaunay tetrahedralization over input vertices
    info("colr_buffer").set(std::move(colr_buffer)); // OpenGL buffer storing texture color positions
    info("vert_buffer").set(std::move(vert_buffer)); // OpenGL buffer storing delaunay vertex positions
    info("elem_buffer").set(std::move(elem_buffer)); // OpenGL buffer storing delaunay tetrahedral elements
    info("bary_buffer").init<gl::Buffer>({ .size = dispatch_n * sizeof(eig::Array4f) }); // Convex weights
  }
  
  bool GenDelaunayWeightsTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info("state", "proj_state").read_only<ProjectState>().verts;
  }

  /* 
    generate weights for full texture
    generate mipmaps from weights
    - either downsample through reduction
    - or compute per level
    - or, given delaunay, represent as sampleable textures, and sample the damned things
    - meanwhile, given generalized, represent as sampleable array textures 
   */

  void GenDelaunayWeightsTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_proj_state = info("state", "proj_state").read_only<ProjectState>();
    const auto &e_appl_data  = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data  = e_appl_data.project_data;

    // Get modified resources
    auto &i_delaunay    = info("delaunay").writeable<AlignedDelaunayData>();
    auto &i_vert_buffer = info("vert_buffer").writeable<gl::Buffer>();
    auto &i_elem_buffer = info("elem_buffer").writeable<gl::Buffer>();

    // Generate new delaunay structure
    std::vector<Colr> delaunay_input(e_proj_data.verts.size());
    std::ranges::transform(e_proj_data.verts, delaunay_input.begin(), [](const auto &vt) { return vt.colr_i; });
    i_delaunay = generate_delaunay<AlignedDelaunayData, Colr>(delaunay_input);
    detail::ImplicitTree<8> tree;
    std::tie(i_delaunay, tree) = detail::generate_search_tree<AlignedDelaunayData, 8>(i_delaunay);

    // Push tree to buffer
    m_tree_buffer = {{ .data = obj_span<const std::byte>(tree.nodes) }};

    // Recover triangle element data and store in project
    auto [_, elems] = convert_mesh<AlignedMeshData>(i_delaunay);
    info.global("appl_data").writeable<ApplicationData>().project_data.elems = elems;

    // Push stale vertices
    auto vert_range = std::views::iota(0u, static_cast<uint>(e_proj_state.verts.size()))
                    | std::views::filter([&](uint i) -> bool { return e_proj_state.verts[i]; });
    for (uint i : vert_range) {
      m_vert_map[i] = e_proj_data.verts[i].colr_i;
      i_vert_buffer.flush(sizeof(eig::AlArray3f), i * sizeof(eig::AlArray3f));
    }

    // Push stale tetrahedral element data // TODO optimize?
    std::ranges::copy(i_delaunay.elems, m_elem_map.begin());
    i_elem_buffer.flush(i_delaunay.elems.size() * sizeof(eig::Array4u));
    
    // Push uniform data
    m_uniform_map->n_verts = i_delaunay.verts.size();
    m_uniform_map->n_elems = i_delaunay.elems.size();
    m_uniform_buffer.flush();

    // Bind required buffers to corresponding targets
    m_program.bind("b_unif", m_uniform_buffer);
    m_program.bind("b_vert", i_vert_buffer);
    m_program.bind("b_elem", i_elem_buffer);
    m_program.bind("b_tree", m_tree_buffer);
    m_program.bind("b_posi", info("colr_buffer").read_only<gl::Buffer>());
    m_program.bind("b_bary", info("bary_buffer").writeable<gl::Buffer>());
    
    // Dispatch shader to generate delaunay convex weights
    gl::dispatch_compute(m_dispatch);
  }
} // namespace met