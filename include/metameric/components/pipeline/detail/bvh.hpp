#pragma once

#include <metameric/core/math.hpp>
#include <limits>
#include <span>
#include <vector>

namespace met::detail {
  // Hardcoded log2 of 8/4/2
  template <uint Degr> consteval uint bvh_degr_log();
  template <> consteval uint bvh_degr_log<2>() { return 1; }
  template <> consteval uint bvh_degr_log<4>() { return 2; }
  template <> consteval uint bvh_degr_log<8>() { return 3; }

  enum class BVHPrimitive {
    ePoint,
    eTriangle,
    eTetrahedron
  };

  // Packed BVH node structure; 2x4b on GL side
  struct BVHNode {
    eig::Array3f minb = std::numeric_limits<float>::max(); // Bounding volume minimum
    uint         i    = 0;                                 // Underlying range begin
    eig::Array3f maxb = std::numeric_limits<float>::min(); // Bounding volume maximum
    uint         n    = 0;                                 // Underlying range extent
  };

  // Simple implicit BVH with padding, supports oc-/quad-/binary structure
  template <uint Degree, BVHPrimitive Ty>
  struct BVH {
    using Node = BVHNode;
    
    constexpr static uint Degr  = Degree;               // Maximum degree for non-leaf nodes
    constexpr static uint LDegr = bvh_degr_log<Degr>(); // Useful constant for build/traverse

  private:
    std::vector<Node> m_nodes;
    uint              m_n_levels;
    uint              m_n_primitives;

  public:
    BVH() = default;

    BVH(std::span<eig::Array3f> vt)
    requires(Ty == BVHPrimitive::ePoint);

    BVH(std::span<eig::Array3f> vt, std::span<eig::Array3u> el)
    requires(Ty == BVHPrimitive::eTriangle);

    BVH(std::span<eig::Array3f> vt, std::span<eig::Array4u> el)
    requires(Ty == BVHPrimitive::eTetrahedron);

  public:
    uint n_levels()     const { return m_n_levels;     };
    uint n_nodes()      const { return m_nodes.size(); };
    uint n_primitives() const { return m_n_primitives; };

  public:
    std::span<const Node> nodes() const { return m_nodes; }
    std::span<Node> nodes() { return m_nodes; }
    std::span<Node> nodes(uint level);
  };
} // namespace met::detail