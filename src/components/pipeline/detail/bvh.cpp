#include <metameric/core/utility.hpp>
#include <metameric/components/pipeline/detail/bvh.hpp>
#include <algorithm>
#include <bit>
#include <execution>
#include <ranges>

namespace met::detail {
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

  // Generate a transforming view that performs unsigned integer index access over a range
  constexpr auto indexed_view(const auto &v) {
    return std::views::transform([&v](uint i) { return v[i]; });
  };

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
    return bvh_lvl_begin<Degr>(lvls + 1); 
  }

  template <uint D, BVHPrimitive Ty>
  BVH<D, Ty>::BVH(std::span<eig::Array3f> vt)
  requires(Ty == BVHPrimitive::ePoint)
  : m_n_primitives(vt.size()),
    m_n_levels(bvh_n_lvls<Degr>(m_n_primitives)),
    m_nodes(bvh_n_nodes<Degr>(m_n_levels), Node { }) {
    met_trace();
    debug::check_expr(false, "Not implemented!");
  }

  template <uint D, BVHPrimitive Ty>
  BVH<D, Ty>::BVH(std::span<eig::Array3f> vt, std::span<eig::Array3u> el)
  requires(Ty == BVHPrimitive::eTriangle)
  : m_n_primitives(el.size()),
    m_n_levels(bvh_n_lvls<Degr>(m_n_primitives)),
    m_nodes(bvh_n_nodes<Degr>(m_n_levels), Node { }) {
    met_trace();
    debug::check_expr(false, "Not implemented!");
  }

  template <uint D, BVHPrimitive Ty>
  BVH<D, Ty>::BVH(std::span<eig::Array3f> vt, std::span<eig::Array4u> el)
  requires(Ty == BVHPrimitive::eTetrahedron)
  : m_n_primitives(el.size()),
    m_n_levels(bvh_n_lvls<Degr>(el.size())) {
    met_trace();
    
    // Prepare node storage
    m_nodes.resize(bvh_n_nodes<Degr>(m_n_levels), Node { });
    m_nodes[0] = { .i = 0, .n = m_n_primitives }; // Root node encompass all primitives
    
    // Build quick and dirty morton order
    std::vector<uint> codes(m_n_primitives), order(m_n_primitives);
    {
      // Generate object centers for primitives, output morton codes
      std::transform(std::execution::par_unseq, range_iter(el), codes.begin(), [&](const eig::Array4u &elem) { 
        return radix::morton_code((vt[elem[0]] + vt[elem[1]] + vt[elem[2]] + vt[elem[3]]) / 4.f); 
      });

      // Generate morton morton order
      // TODO; get a radix sort in here, dammit
      std::iota(range_iter(order), 0u);
      std::sort(std::execution::par_unseq, range_iter(order), [&](uint i, uint j) { return codes[i] < codes[j]; });
    }

    // Adjust input data to adhere to order
    // TODO; instead of doing hidden stuff, provide a copy of the data
    std::vector<eig::Array4u> el_(range_iter(el));
    std::transform(std::execution::par_unseq, range_iter(order), el.begin(), [&](uint i) { return el_[i]; });
    
    // Initialize root node; encompass all primitives
    m_nodes[0] = { .i = 0, .n = m_n_primitives };

    // Build subdivision based on morton order; push down from root
    for (int lvl = 0; lvl < m_n_levels - 1; ++lvl) {
      // Iterate over nodes on level
      #pragma omp parallel for
      for (int i = bvh_lvl_begin<Degr>(lvl); i < bvh_lvl_end<Degr>(lvl); ++i) {
        const Node &node = m_nodes[i];

        std::vector<Node> children = { node };
        for (int j = LDegr; j > 0; --j) { // 3, 2, 1
          std::vector<Node> _children;
          for (auto &child : children) {
            // Propagate leaf/empty nodes to bottom of tree
            if (child.n <= 1) {
              _children.push_back(child);
              continue;
            }

            // Determine ranges of left/right child nodes
            uint begin = child.i, end = begin + child.n - 1;
            uint split = radix::find_split(codes, begin, end);
            
            // Push new child nodes
            _children.push_back({ .i = begin,     .n = split - begin + 1 });
            _children.push_back({ .i = split + 1, .n = end - split });
          } // for node
          children = _children;  
        } // for j
        
        // Store subdivided result in tree
        std::ranges::copy(children, m_nodes.begin() + i * Degr + 1);
      } // for i
    } // for lvl

    // Build bounding data based on primitives; pull up from leaves 
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
          for (const auto &child : std::views::iota(1 + i * Degr, 1 + (i + 1) * Degr)
                                 | indexed_view(m_nodes)) {
            node.minb = node.minb.cwiseMin(child.minb);
            node.maxb = node.maxb.cwiseMax(child.maxb);
          }
        }
      } // for i
    } // for lvl
  }

  template <uint D, BVHPrimitive Ty>
  std::span<typename BVH<D, Ty>::Node> BVH<D, Ty>::nodes(uint level) {
    return { m_nodes.begin() + bvh_lvl_begin<Degr>(level), bvh_lvl_size<Degr>(level) };
  }

  /* Explicit template declarations */

  template class BVH<2, BVHPrimitive::ePoint>;
  template class BVH<4, BVHPrimitive::ePoint>;
  template class BVH<8, BVHPrimitive::ePoint>;
  
  template class BVH<2, BVHPrimitive::eTriangle>;
  template class BVH<4, BVHPrimitive::eTriangle>;
  template class BVH<8, BVHPrimitive::eTriangle>;

  template class BVH<2, BVHPrimitive::eTetrahedron>;
  template class BVH<4, BVHPrimitive::eTetrahedron>;
  template class BVH<8, BVHPrimitive::eTetrahedron>;
} // namespace met::detail