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

  // Type of data primitive over which the hierarchy is constructed
  enum class BVHPrimitive {
    ePoint,
    eTriangle,
    eTetrahedron
  };

  // Packed ball-tree node structure; packed 6b on GL side
  struct BTNode {
    eig::Array3f p = 0; // Sphere center
    float        r = 0; // Sphere radius
    uint         i = 0; // Underlying range begin
    uint         n = 0; // Underlying range extent
  };

  // Packed bounding-volume-hierarchy node structure; packed 8b on GL side
  struct BVHNode {
    eig::Array3f minb = std::numeric_limits<float>::max(); // Bounding volume minimum
    uint         i    = 0;                                 // Underlying range begin
    eig::Array3f maxb = std::numeric_limits<float>::min(); // Bounding volume maximum
    uint         n    = 0;                                 // Underlying range extent
  };

  // Simple implicit BVH with padding, supports oc-/quad-/binary structure
  template <
    typename VertTy,
    typename NodeTy,
    uint Degree, 
    BVHPrimitive Ty
  > struct BVH {
    using Node = NodeTy;
    using Vert = VertTy;
    
    constexpr static uint Degr  = Degree;               // Maximum degree for non-leaf nodes
    constexpr static uint LDegr = bvh_degr_log<Degr>(); // Useful constant for build/traverse

  private:
    std::vector<Node> m_nodes;
    uint              m_n_levels;
    uint              m_n_primitives;

  public:
    // Default constructor
    BVH() = default;

    // Reserving constructor; simply reserves space for maximum nr. of primitives
    BVH(uint max_primitives);

    // Building constructors for different primitive types
    BVH(std::span<Vert> vt)                             requires(Ty == BVHPrimitive::ePoint);
    BVH(std::span<Vert> vt, std::span<eig::Array3u> el) requires(Ty == BVHPrimitive::eTriangle);
    BVH(std::span<Vert> vt, std::span<eig::Array4u> el) requires(Ty == BVHPrimitive::eTetrahedron);

    // Reserve space without rebuild
    void reserve(uint max_primitives);

    // Build functions for different primitive types
    void build(std::span<Vert> vt)                             requires(Ty == BVHPrimitive::ePoint);
    void build(std::span<Vert> vt, std::span<eig::Array3u> el) requires(Ty == BVHPrimitive::eTriangle);
    void build(std::span<Vert> vt, std::span<eig::Array4u> el) requires(Ty == BVHPrimitive::eTetrahedron);

  public:
    uint n_levels()     const { return m_n_levels;     };
    uint n_primitives() const { return m_n_primitives; };

  public:
    size_t size()                const { return data().size();   };
    size_t size_reserved()       const { return m_nodes.size();  };
    size_t size_bytes()          const { return data().size_bytes();           }
    size_t size_bytes_reserved() const { return m_nodes.size() * sizeof(Node); }

  public:
    std::span<Node>       data();
    std::span<const Node> data() const;
    std::span<Node>       data(uint level);
    std::span<const Node> data(uint level) const;
  };

  template <uint DegreeA, uint DegreeB>
  std::vector<eig::Array2u> init_pair_data(uint level_a, uint level_b);
} // namespace met::detail