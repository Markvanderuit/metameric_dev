#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/openmesh.hpp>
#include <span>
#include <vector>
#include <unordered_map>
#include <utility>

namespace met {
  /* OpenMesh begins here */

  // Mesh type: triangle mesh
  // Mesh kernel: array kernel
  // Mesh traits: specify types

  /* An indexed mesh with face normals and no additional data */
  struct BaselineMeshTraits : public omesh::DefaultTraits {
    // Define default attributes; only face normals are stored, half-edges are intentionally omitted
    VertexAttributes(omesh::Attributes::None);
    HalfedgeAttributes(omesh::Attributes::None);
    FaceAttributes(omesh::Attributes::None);
  };

  /* An indexed mesh with face normals */
  struct FNormalMeshTraits : public omesh::DefaultTraits {
    VertexAttributes(omesh::Attributes::None);
    HalfedgeAttributes(omesh::Attributes::None);
    FaceAttributes(omesh::Attributes::Normal);
  };

  /* An indexed mesh with face normals and half-edges to simplify a number of operations */
  struct HalfedgeMeshTraits : public omesh::DefaultTraits {
    VertexAttributes(omesh::Attributes::None);
    HalfedgeAttributes(omesh::Attributes::PrevHalfedge);
    FaceAttributes(omesh::Attributes::Normal);
  };

  // Triangle mesh shorthands implementing the above defined traits
  using BaselineMesh = omesh::TriMesh_ArrayKernelT<BaselineMeshTraits>;
  using FNormalMesh   = omesh::TriMesh_ArrayKernelT<FNormalMeshTraits>;
  using HalfedgeMesh = omesh::TriMesh_ArrayKernelT<HalfedgeMeshTraits>;


  struct GamutMeshTraits : public omesh::DefaultTraits {
    // Use internal types; there are many bugs with Eigen and e.g. the OpenMesh minimization framework
    using Point    = omesh::Vec3f;
    using Normal   = omesh::Vec3f;

  };

  using GamutMesh = omesh::TriMesh_ArrayKernelT<GamutMeshTraits>;

  /* OpenMesh ends here */

  // FWD
  template <typename T, typename E = eig::Array3u>
  struct IndexedMesh;

  /* Simple indexed triangle mesh structure */
  template <typename T, typename E>
  struct IndexedMesh {
    using Vert = T;
    using Elem = E;

  private:
    std::vector<Vert> m_verts;
    std::vector<Elem> m_elems;

  public:
    IndexedMesh() = default;
    IndexedMesh(std::span<const Vert> verts, std::span<const Elem> elems);

    // Accessors
    std::vector<Vert>& verts() { return m_verts; }
    std::vector<Elem>& elems() { return m_elems; }
    const std::vector<Vert>& verts() const { return m_verts; }
    const std::vector<Elem>& elems() const { return m_elems; }

    // Algorithms

    T centroid() const;
  };

  using Array3fMesh        = IndexedMesh<eig::Array3f, eig::Array3u>;
  using AlArray3fMesh      = IndexedMesh<eig::AlArray3f, eig::Array3u>;

  // Generate a subdivided octahedron whose vertices lie on a unit sphere
  template <typename T = eig::AlArray3f>
  IndexedMesh<T> generate_unit_sphere(uint n_subdivs = 3);

  // Generate an approximate convex hull from a mesh describing a unit sphere
  // by matching each vertex to a point
  template <typename T = eig::AlArray3f>
  IndexedMesh<T, eig::Array3u> generate_convex_hull(const IndexedMesh<T, eig::Array3u> &sphere_mesh, 
                                                    std::span<const T> points,
                                                    float threshold,
                                                    float max_error);

  /* Convex hull functions */

  // Shorthand that first generates a sphere mesh
  template <typename T = eig::AlArray3f>
  IndexedMesh<T, eig::Array3u> generate_convex_hull(std::span<const T> points);

  template <typename T = eig::AlArray3f>
  bool is_point_inside_convex_hull(const IndexedMesh<T, eig::Array3u> &chull, const T &test_point);

  /* Miscellaneous functions */

  // Generate a wireframe mesh from an input triangle mesh
  template <typename T = eig::AlArray3f>
  IndexedMesh<T, eig::Array2u> generate_wireframe(const IndexedMesh<T, eig::Array3u> &mesh);

  // Perform progressive mesh simplification by edge collapse until vertex count <= max_vertices
  template <typename T = eig::AlArray3f>
  IndexedMesh<T, eig::Array3u> simplify_mesh(const IndexedMesh<T, eig::Array3u> &mesh, uint max_vertices);
} // namespace met