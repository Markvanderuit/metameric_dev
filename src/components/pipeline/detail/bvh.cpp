#include <metameric/core/utility.hpp>
#include <metameric/components/pipeline/detail/bvh.hpp>
#include <algorithm>
#include <bit>
#include <bitset>
#include <deque>
#include <execution>
#include <numeric>
#include <functional>
#include <ranges>

namespace met::detail {
  // Generate a transforming closure that performs unsigned integer index access in some object
  constexpr auto indexed_closure(const auto &v) {
    return [&v](uint i) { return v[i]; };
  };

  // Generate a transforming view that performs unsigned integer index access in some object
  constexpr auto indexed_view(const auto &v) {
    return std::views::transform(indexed_closure(v));
  };

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
    uint morton_code(eig::Array3f v_) {
      auto v = (v_ * 1024.f).cwiseMax(0.f).cwiseMin(1023.f).eval();
      uint x = expand_bits_10(uint(v[0]));
      uint y = expand_bits_10(uint(v[1]));
      uint z = expand_bits_10(uint(v[2]));
      return x * 4u + y * 2u + z;
    }

    inline
    int find_msb(uint i) {
      return 31 - std::countl_zero(i);
    }

    // Find split position within a range [first, last] of morton codes
    inline
    uint find_split(std::span<const uint> codes, uint first, uint last) {
      // Initial guess for split position
      uint split = first;

      // Get data for initial guess 
      uint code = codes[first];
      uint pref = find_msb(code ^ codes[last]);

      // Perform a binary search to find the split position
      uint step = last - first;
      do {
        step = (step + 1) >> 1;        // Decrease step size
        uint new_split = split + step; // Possible new split position

        guard_continue(new_split < last);
        guard_continue(find_msb(code ^ codes[new_split]) < pref);

        split = new_split; // Accept newly proposed split for this iteration
      } while (step > 1);

      return split;
      /* return first + (last - first) / 2; // blunt halfway split for testing */
    }
  } // namespace radix

  template <typename NodeTy, typename VertTy> 
  void reduce_leaf_point(NodeTy &node, std::span<const VertTy> vts);
  template <typename NodeTy, typename VertTy> 
  void reduce_node_point(NodeTy &node, const NodeTy &child, std::span<const VertTy> underlying_vts);

  template <>
  void reduce_leaf_point<BTNode, eig::Array3f>(BTNode &node, std::span<const eig::Array3f> vts) {
    guard(node.n > 0);

    // Single point, return point
    // >= 2 points, we use Jack Ritter's bounding sphere approximation
    if (node.n == 1) {
      node.p = vts[node.i];
      node.r = 0.f;
      return;
    }
    
    // Get contiguous range over contained points
    auto view_range = std::views::iota(node.i, node.i + node.n) | indexed_view(vts);
    std::vector<eig::Array3f> view(range_iter(view_range));

    // Select starting point from point set, and find a farthest point
    auto p_begin = view.front();
    auto p_begin_farthest = *std::max_element(view.begin() + 1, view.end(), [&p_begin](const auto &vt_a, const auto &vt_b) {
      return (vt_a - p_begin).matrix().squaredNorm() < (vt_b - p_begin).matrix().squaredNorm();
    });

    // Next, find the farthest point from that farthest point
    auto p_next_farthest = *std::max_element(view.begin() + 1, view.end(), [&p_begin_farthest](const auto &vt_a, const auto &vt_b) {
      return (vt_a - p_begin_farthest).matrix().squaredNorm() < (vt_b - p_begin_farthest).matrix().squaredNorm();
    });

    // Assign initial bounding sphere between these farthest points
    node.p = 0.5f * (p_begin_farthest + p_next_farthest);
    node.r = 0.5f * (p_begin_farthest - p_next_farthest).matrix().norm();
    
    // Next, for any other points that lie outside the bounding sphere, grow the sphere to encompass those points.
    for (const auto &vt : view)
      node.r = std::max(node.r, (vt - node.p).matrix().norm());
  }

  template <>
  void reduce_leaf_point<BTNode, eig::AlArray3f>(BTNode &node, std::span<const eig::AlArray3f> vts) {
    eig::Array3f minv = std::numeric_limits<float>::max(), 
                 maxv = std::numeric_limits<float>::min(),
                 posv = 0;

    for (const auto &vt : std::views::iota(node.i, node.i + node.n) | indexed_view(vts)) {
      minv = minv.cwiseMin(vt);
      maxv = maxv.cwiseMin(vt);
      posv += vt;
    }
    
    node.p = posv / static_cast<float>(node.n);
    node.r = 0.5f * (maxv - minv).matrix().norm();
  }

  template <>
  void reduce_leaf_point<BVHNode, eig::Array3f>(BVHNode &node, std::span<const eig::Array3f> vts) {
    for (const auto &vt : std::views::iota(node.i, node.i + node.n) | indexed_view(vts)) {
      node.minb = node.minb.cwiseMin(vt);
      node.maxb = node.maxb.cwiseMax(vt);
    }
  }

  template <>
  void reduce_leaf_point<BVHNode, eig::AlArray3f>(BVHNode &node, std::span<const eig::AlArray3f> vts) {
    for (const auto &vt : std::views::iota(node.i, node.i + node.n) | indexed_view(vts)) {
      node.minb = node.minb.cwiseMin(vt);
      node.maxb = node.maxb.cwiseMax(vt);
    }
  }

  template <>
  void reduce_node_point<BVHNode, eig::Array3f>(BVHNode &node, const BVHNode &child, std::span<const eig::Array3f> _vts) {
    node.minb = node.minb.cwiseMin(child.minb);
    node.maxb = node.maxb.cwiseMax(child.maxb);
  }

  template <>
  void reduce_node_point<BVHNode, eig::AlArray3f>(BVHNode &node, const BVHNode &child, std::span<const eig::AlArray3f> _vts) {
    node.minb = node.minb.cwiseMin(child.minb);
    node.maxb = node.maxb.cwiseMax(child.maxb);
  }

  // Padded begin index on current tree level
  template <uint Degr> constexpr uint bvh_lvl_begin(int lvl);
  template <> constexpr uint bvh_lvl_begin<2>(int lvl) { return 0xFFFFFFFF >> (32 - (lvl - 1)); }
  template <> constexpr uint bvh_lvl_begin<4>(int lvl) { return 0x55555555 >> (31 - ((bvh_degr_log<4>() * lvl)) + 1); }
  template <> constexpr uint bvh_lvl_begin<8>(int lvl) { return (0x92492492 >> (31 - bvh_degr_log<8>() * lvl)) >> 3; }

  // Padded extent of a tree level
  template <uint Degr> constexpr uint bvh_lvl_size(uint lvl) {
    return bvh_lvl_begin<Degr>(lvl + 1) - bvh_lvl_begin<Degr>(lvl);
  }

  // Padded end index on current tree level
  template <uint Degr> constexpr uint bvh_lvl_end(uint lvl) {
    return bvh_lvl_begin<Degr>(lvl + 1);
  }

  // Padded nr. of levels, given nr. of primitives
  template <uint Degr> constexpr uint bvh_n_lvls(uint n) {
    return 1 + static_cast<uint>(std::ceil(std::log2(n) / bvh_degr_log<Degr>()));
  }
    
  // Padded nr. of nodes, given nr. of levels
  template <uint Degr> constexpr uint bvh_n_nodes(uint lvls) { 
    return bvh_lvl_begin<Degr>(lvls); 
  }

  template <typename Vt, typename Node, uint D, BVHPrimitive Ty>
  BVH<Vt, Node, D, Ty>::BVH(uint max_primitives) {
    met_trace();
    reserve(max_primitives);
  }

  template <typename Vt, typename Node, uint D, BVHPrimitive Ty>
  BVH<Vt, Node, D, Ty>::BVH(std::span<const Vt> vt)
  requires(Ty == BVHPrimitive::ePoint) {
    met_trace();
    build(vt);
  }

  template <typename Vt, typename Node, uint D, BVHPrimitive Ty>
  BVH<Vt, Node, D, Ty>::BVH(std::span<const Vt> vt, std::span<const eig::Array3u> el)
  requires(Ty == BVHPrimitive::eTriangle) {
    met_trace();
    build(vt, el);
  }

  template <typename Vt, typename Node, uint D, BVHPrimitive Ty>
  BVH<Vt, Node, D, Ty>::BVH(std::span<const Vt> vt, std::span<const eig::Array4u> el)
  requires(Ty == BVHPrimitive::eTetrahedron) {
    met_trace();
    build(vt, el);
  }

  template <typename Vt, typename Node, uint D, BVHPrimitive Ty>
  void BVH<Vt, Node, D, Ty>::reserve(uint max_primitives) {
    met_trace();
    m_nodes.resize(bvh_n_nodes<Degr>(bvh_n_lvls<Degr>(max_primitives)));
  }

  template <typename Vt, typename Node, uint D, BVHPrimitive Ty>
  void BVH<Vt, Node, D, Ty>::build(std::span<const Vt> vt_)
  requires(Ty == BVHPrimitive::ePoint) {
    met_trace();
    
    m_n_primitives = vt_.size();
    m_n_levels = bvh_n_lvls<Degr>(m_n_primitives);
    m_order.resize(m_n_primitives);
    if (m_nodes.size() < bvh_n_nodes<Degr>(m_n_levels))
      reserve(m_n_primitives);

    // Initialize or clear data
    std::fill(std::execution::par_unseq, range_iter(m_nodes), Node { });
    m_nodes[0] = { .i = 0, .n = m_n_primitives };

    // Build quick and dirty morton order
    std::vector<uint> codes(m_n_primitives);
    { met_trace_n("order_generation");
      // Generate object centers for primitives, output morton codes
      std::transform(std::execution::par_unseq, range_iter(vt_), codes.begin(),
        [](const auto &v) { return radix::morton_code(v); });

      // Generate morton order // TODO; get a radix sort in here, dammit
      std::iota(range_iter(m_order), 0u);
      std::sort(std::execution::par_unseq, range_iter(m_order), [&](uint i, uint j) { return codes[i] < codes[j]; });

      // Adjust code data to adhere to order
      std::vector<uint> codes_(codes);
      std::transform(std::execution::par_unseq, range_iter(m_order), codes.begin(), indexed_closure(codes_));
    } // order_generation

    // Temporary sorted vertex order
    std::vector<Vert> vt(m_n_primitives);
    std::transform(std::execution::par_unseq, range_iter(m_order), vt.begin(), indexed_closure(vt_));

    // Build subdivision based on morton order; push down from root
    { met_trace_n("subdivision");
      for (int lvl = 0; lvl < m_n_levels - 1; ++lvl) {
        // Iterate over nodes on level
        #pragma omp parallel for
        for (int i = bvh_lvl_begin<Degr>(lvl); i < bvh_lvl_end<Degr>(lvl); ++i) {
          const Node &node = m_nodes[i];

          // Generate subdivided nodes, or propagate current node
          std::vector<Node> children = { node };
          for (int j = LDegr; j > 0; --j) { // 3, 2, 1
            guard_break(children.size() > 1 || children[0].n > 1);

            std::vector<Node> children_subdivided;
            for (auto &child : children) {
              // Propagate leaf/empty nodes to bottom of tree
              if (child.n <= 1) {
                children_subdivided.push_back(child);
                continue;
              }

              // Determine ranges of left/right child nodes
              uint begin = child.i, end = begin + child.n - 1;
              uint split = radix::find_split(codes, begin, end);
              
              // Push new child nodes
              children_subdivided.push_back({ .i = begin,     .n = split - begin + 1 });
              children_subdivided.push_back({ .i = split + 1, .n = end - split });
            } // for node
            children = children_subdivided;  
          } // for j
          
          // Store subdivided result in tree
          std::ranges::copy(children, m_nodes.begin() + 1 + i * Degr);
        } // for i
      } // for lvl
    } // subdivision

    // Build bounding data based on primitives; pull up from leaves 
    { met_trace_n("bounding_volumes");
      for (int lvl = m_n_levels - 1; lvl >= 0; --lvl) {
        // Iterate over nodes on level
        #pragma omp parallel for
        for (int i = bvh_lvl_begin<Degr>(lvl); i < bvh_lvl_end<Degr>(lvl); ++i) {
          Node &node = m_nodes[i];
          guard_continue(node.n > 0); // Skip empty padding nodes

          // Build data; separate into leaf/non-leaf cases 
          if (node.n == 1 || lvl == m_n_levels - 1) {  // Leaf node; 
            // Fit initial bounding volume
            for (const auto &vert : std::views::iota(node.i, node.i + node.n) 
                                | indexed_view(vt)) {
              node.minb = node.minb.cwiseMin(vert);
              node.maxb = node.maxb.cwiseMax(vert);
            }
            // reduce_leaf_point(node, std::span<const Vert> { vt });
          } else {  // Non-leaf node; 
            // Generate folded bounding volume (over non-empty children)
            for (const auto &child : std::views::iota(1 + i * Degr, 1 + (i + 1) * Degr)
                                   | indexed_view(m_nodes)) {
              node.minb = node.minb.cwiseMin(child.minb);
              node.maxb = node.maxb.cwiseMax(child.maxb);
            }

            /* auto children = std::views::iota(1 + i * Degr, 1 + (i + 1) * Degr) | indexed_view(m_nodes);
            // reduce_node_point(node, child, std::span<const Vert> { vt });
            for (const auto &child : children) {
              node.minb = node.minb.cwiseMin(child.minb);
              node.maxb = node.maxb.cwiseMax(child.maxb);
            } */
          }
        } // for i
      } // for lvl
    } // bounding_volume_generation
  }
  
  template <typename Vt, typename Node, uint D, BVHPrimitive Ty>
  void BVH<Vt, Node, D, Ty>::build(std::span<const Vt> vt, std::span<const eig::Array3u> el)
  requires(Ty == BVHPrimitive::eTriangle) {
    met_trace();
    debug::check_expr(false, "Not implemented!");
  }

  template <typename Vt, typename Node, uint D, BVHPrimitive Ty>
  void BVH<Vt, Node, D, Ty>::build(std::span<const Vt> vt, std::span<const eig::Array4u> el_)
  requires(Ty == BVHPrimitive::eTetrahedron) {
    met_trace();

    m_n_primitives = el_.size();
    m_n_levels = bvh_n_lvls<Degr>(m_n_primitives);
    m_order.resize(m_n_primitives);
    if (m_nodes.size() < bvh_n_nodes<Degr>(m_n_levels))
      reserve(m_n_primitives);
    
    // Initialize or clear data
    std::fill(std::execution::par_unseq, range_iter(m_nodes), Node { });
    m_nodes[0] = { .i = 0, .n = m_n_primitives };

    // Build quick and dirty morton order
    std::vector<uint> codes(m_n_primitives);
    { met_trace_n("order_generation");
      // Generate object centers for primitives, output morton codes
      std::transform(std::execution::par_unseq, range_iter(el_), codes.begin(), [&](const eig::Array4u &elem) { 
        auto verts = elem | indexed_view(vt);
        eig::Array3f maxb = std::reduce(range_iter(verts), m_nodes[0].maxb, [](auto a, auto b) { return a.max(b); });
        eig::Array3f minb = std::reduce(range_iter(verts), m_nodes[0].minb, [](auto a, auto b) { return a.min(b); });
        return radix::morton_code((minb + maxb) * 0.5);
      });

      // Generate morton order // TODO; get a radix sort in here, dammit
      std::iota(range_iter(m_order), 0u);
      std::sort(std::execution::par_unseq, range_iter(m_order), [&](uint i, uint j) { return codes[i] < codes[j]; });

      // Adjust code data to adhere to order
      std::vector<uint> codes_(codes);
      std::transform(std::execution::par_unseq, range_iter(m_order), codes.begin(), indexed_closure(codes_));
    } // order_generation

    // Temporary sorted element order
    std::vector<eig::Array4u> el(m_n_primitives);
    std::transform(std::execution::par_unseq, range_iter(m_order), el.begin(), indexed_closure(el_));
    
    // Build subdivision based on morton order; push down from root
    { met_trace_n("subdivision");
      for (int lvl = 0; lvl < m_n_levels - 1; ++lvl) {
        // Iterate over nodes on level
        #pragma omp parallel for
        for (int i = bvh_lvl_begin<Degr>(lvl); i < bvh_lvl_end<Degr>(lvl); ++i) {
          const Node &node = m_nodes[i];

          // Generate subdivided nodes, or propagate current node
          std::vector<Node> children = { node };
          for (int j = LDegr; j > 0; --j) { // 3, 2, 1
            guard_break(children.size() > 1 || children[0].n > 1);
            
            std::vector<Node> children_subdivided;
            for (const auto &child : children) {
              // Propagate leaf/empty nodes to bottom of tree
              if (child.n <= 1) {
                children_subdivided.push_back(child);
                continue;
              }

              // Determine ranges of left/right child nodes
              uint begin = child.i, end = begin + child.n - 1;
              uint split = radix::find_split(codes, begin, end);
              
              // Push new child nodes
              children_subdivided.push_back({ .i = begin,     .n = split - begin + 1 });
              children_subdivided.push_back({ .i = split + 1, .n = end - split });
            } // for node
            children = children_subdivided;  
          } // for j
          
          // Store subdivided result in tree
          std::ranges::copy(children, m_nodes.begin() + 1 + i * Degr);
        } // for i
      } // for lvl
    } // subdivision

    // Build bounding data based on primitives; pull up from leaves 
    { met_trace_n("bounding_volumes");
      for (int lvl = m_n_levels - 1; lvl >= 0; --lvl) {
        // Iterate over nodes on level
        #pragma omp parallel for
        for (int i = bvh_lvl_begin<Degr>(lvl); i < bvh_lvl_end<Degr>(lvl); ++i) {
          Node &node = m_nodes[i];
          guard_continue(node.n > 0); // Skip empty padding nodes

          // Build data; separate into leaf/non-leaf cases 
          if (node.n == 1 || lvl == m_n_levels - 1) {  // Leaf node; 
            // Fit bounding volume around contained mesh vertices
            for (const auto &vert : std::views::iota(node.i, node.i + node.n)
                                  | indexed_view(el) 
                                  | std::views::join 
                                  | indexed_view(vt)) {
              node.minb = node.minb.cwiseMin(vert);
              node.maxb = node.maxb.cwiseMax(vert);
            }
          } else {  // Non-leaf node; 
            // Generate bounding volume over non-empty children
            auto children = std::views::iota(1 + i * Degr, 1 + (i + 1) * Degr) | indexed_view(m_nodes);
            
            for (const auto &child : std::views::iota(1 + i * Degr, 1 + (i + 1) * Degr)
                                   | indexed_view(m_nodes)) {
              node.minb = node.minb.cwiseMin(child.minb);
              node.maxb = node.maxb.cwiseMax(child.maxb);
            }
          }
        } // for i
      } // for lvl
    } // bounding_volume_generation
  }

  template <typename Vt, typename Node, uint D, BVHPrimitive Ty>
  std::span<typename BVH<Vt, Node, D, Ty>::Node> BVH<Vt, Node, D, Ty>::data() {
    met_trace();
    return std::span<Node> { m_nodes.begin(), bvh_n_nodes<Degr>(m_n_levels) };
  }

  template <typename Vt, typename Node, uint D, BVHPrimitive Ty>
  std::span<const typename BVH<Vt, Node, D, Ty>::Node> BVH<Vt, Node, D, Ty>::data() const {
    met_trace();
    return std::span<const Node> { m_nodes.begin(), bvh_n_nodes<Degr>(m_n_levels) };
  }

  template <typename Vt, typename Node, uint D, BVHPrimitive Ty>
  std::span<typename BVH<Vt, Node, D, Ty>::Node> BVH<Vt, Node, D, Ty>::data(uint level) {
    met_trace();
    return std::span<Node> { m_nodes.begin() + bvh_lvl_begin<Degr>(level), bvh_lvl_size<Degr>(level) };
  }

  template <typename Vt, typename Node, uint D, BVHPrimitive Ty>
  std::span<const typename BVH<Vt, Node, D, Ty>::Node> BVH<Vt, Node, D, Ty>::data(uint level) const {
    met_trace();
    return std::span<const Node> { m_nodes.begin() + bvh_lvl_begin<Degr>(level), bvh_lvl_size<Degr>(level) };
  }

  template <typename Vt, typename Node, uint D, BVHPrimitive Ty>
  std::span<const uint> BVH<Vt, Node, D, Ty>::order() const {
    met_trace();
    return std::span<const uint> { m_order };
  }
  
  template <uint DegrA, uint DegrB>
  std::vector<eig::Array2u> init_pair_data(uint level_a, uint level_b) {
    uint begin_a = bvh_lvl_begin<DegrA>(level_a);
    uint begin_b = bvh_lvl_begin<DegrB>(level_b);
    uint size_a = bvh_lvl_size<DegrA>(level_a);
    uint size_b = bvh_lvl_size<DegrB>(level_b);

    std::vector<eig::Array2u> data(size_a * size_b);
    for (uint i = 0; i < size_a; ++i) {
      for (uint j = 0; j < size_b; ++j) {
        data[i * size_b + j] = { begin_a + i, begin_b + j };
      }
    }
    return data;
  }

  /* Explicit template declarations */

  template class BVH<eig::Array3f, BVHNode, 2, BVHPrimitive::ePoint>;
  template class BVH<eig::Array3f, BVHNode, 4, BVHPrimitive::ePoint>;
  template class BVH<eig::Array3f, BVHNode, 8, BVHPrimitive::ePoint>;
  
  template class BVH<eig::Array3f, BVHNode, 2, BVHPrimitive::eTriangle>;
  template class BVH<eig::Array3f, BVHNode, 4, BVHPrimitive::eTriangle>;
  template class BVH<eig::Array3f, BVHNode, 8, BVHPrimitive::eTriangle>;

  template class BVH<eig::Array3f, BVHNode, 2, BVHPrimitive::eTetrahedron>;
  template class BVH<eig::Array3f, BVHNode, 4, BVHPrimitive::eTetrahedron>;
  template class BVH<eig::Array3f, BVHNode, 8, BVHPrimitive::eTetrahedron>;

  template class BVH<eig::AlArray3f, BVHNode, 2, BVHPrimitive::ePoint>;
  template class BVH<eig::AlArray3f, BVHNode, 4, BVHPrimitive::ePoint>;
  template class BVH<eig::AlArray3f, BVHNode, 8, BVHPrimitive::ePoint>;
  
  template class BVH<eig::AlArray3f, BVHNode, 2, BVHPrimitive::eTriangle>;
  template class BVH<eig::AlArray3f, BVHNode, 4, BVHPrimitive::eTriangle>;
  template class BVH<eig::AlArray3f, BVHNode, 8, BVHPrimitive::eTriangle>;

  template class BVH<eig::AlArray3f, BVHNode, 2, BVHPrimitive::eTetrahedron>;
  template class BVH<eig::AlArray3f, BVHNode, 4, BVHPrimitive::eTetrahedron>;
  template class BVH<eig::AlArray3f, BVHNode, 8, BVHPrimitive::eTetrahedron>;

  template std::vector<eig::Array2u> init_pair_data<8, 8>(uint, uint);
} // namespace met::detail