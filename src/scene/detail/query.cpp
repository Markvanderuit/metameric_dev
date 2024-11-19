#include <metameric/core/ranges.hpp>
#include <metameric/scene/detail/query.hpp>

namespace met::detail {
  // From a trio of vertex positions forming a triangle, and a center position,
  // compute barycentric coordinate representation
  eig::Vector3f get_barycentric_coords(eig::Vector3f p, eig::Vector3f a, eig::Vector3f b, eig::Vector3f c) {
    met_trace();

    eig::Vector3f ab = b - a, ac = c - a;

    float a_tri = std::abs(.5f * ac.cross(ab).norm());
    float a_ab  = std::abs(.5f * (p - a).cross(ab).norm());
    float a_ac  = std::abs(.5f * ac.cross(p - a).norm());
    float a_bc  = std::abs(.5f * (c - p).cross(b - p).norm());

    return (eig::Vector3f(a_bc, a_ac, a_ab) / a_tri).eval();
  }

  SurfaceInfo query_surface_info(const Scene &scene, const eig::Array3f p, const SurfaceRecord &rc) {
    met_trace();

    // Get relevant resources; some objects, some caches of gl-side data
    const auto &object    = *scene.components.objects[rc.object_i()];
    const auto &uplifting = *scene.components.upliftings[object.uplifting_i];
    const auto &mesh_data = scene.resources.meshes.gl.mesh_cache[object.mesh_i];
    
    // Get object*mesh transform and inverse
    auto object_trf = object.transform.affine().matrix().eval();
    auto mesh_trf   = mesh_data.unit_trf;
    auto trf        = (object_trf * mesh_trf).eval();
    
    // Assemble SurfaceInfo object
    SurfaceInfo si = { 
      .object_i    = rc.object_i(),
      .uplifting_i = object.uplifting_i,
      .record      = rc 
    };

    // Assemble relevant primitive data from mesh cache
    std::array<detail::Vertex, 3> prim;
    {
      // Extract element indices, un-scattering data from the weird BVH leaf node order
      const Mesh &mesh = mesh_data.mesh;
      const auto &blas = mesh_data.bvh;
      const auto &elem = mesh.elems[blas.prims[rc.primitive_i() - mesh_data.prims_offs]]; 
      
      // Iterate element indices, building primitive vertices
      for (uint i = 0; i < 3; ++i) {
        uint j = elem[i];
        
        // Get vertex data, and force UV to [0, 1]
        auto vert = mesh.verts[j];
        auto norm = mesh.has_norms() ? mesh.norms[j] : eig::Array3f(0, 0, 1);
        auto txuv = mesh.has_txuvs() ? mesh.txuvs[j] : eig::Array2f(0.5);
        txuv = txuv.unaryExpr([](float f) {
          int   i = static_cast<int>(f);
          float a = f - static_cast<float>(i);
          return (i % 2) ? 1.f - a : a;
        });

        // Assemble primitive vertex
        prim[i] = { .p = vert, .n = norm, .tx = txuv };
      }
    }

    // Generate barycentric coordinates
    eig::Vector3f pinv = (trf.inverse() * eig::Vector4f(p.x(), p.y(), p.z(), 1.f)).head<3>();
    eig::Vector3f bary = detail::get_barycentric_coords(pinv, prim[0].p, prim[1].p, prim[2].p);

    // Recover surface geometric data
    si.p  = bary.x() * prim[0].p  + bary.y() * prim[1].p  + bary.z() * prim[2].p;
    si.n  = bary.x() * prim[0].n  + bary.y() * prim[1].n  + bary.z() * prim[2].n;
    si.tx = bary.x() * prim[0].tx + bary.y() * prim[1].tx + bary.z() * prim[2].tx;
    si.p  = (trf * eig::Vector4f(si.p.x(), si.p.y(), si.p.z(), 1.f)).head<3>();
    si.n  = (trf * eig::Vector4f(si.n.x(), si.n.y(), si.n.z(), 0.f)).head<3>();
    si.n.normalize();

    // Recover surface diffuse data based on underlying object material
    si.diffuse = object.diffuse | visit {
      [&](uint i) -> Colr { 
        return scene.resources.images[i]->sample(si.tx, Image::ColorFormat::eLRGB).head<3>(); 
      },
      [&](Colr c) -> Colr { 
        return c; 
      }
    };

    return si;
  }

  UpliftingInfo query_uplifting_info(const Scene &scene, const SurfaceInfo &si) {
    met_trace();

    // Get relevant resources; tessellation, barycentric coordinates, index of enclosing tetrahedron
    const auto &uplifting_data = scene.components.upliftings.gl.uplifting_data[si.uplifting_i];
    const auto &tessellation   = uplifting_data.tessellation;
    auto [bary, tetr_i]        = uplifting_data.find_enclosing_tetrahedron(si.diffuse);

    // Return value
    UpliftingInfo tr = { 
      .uplifting_i   = si.uplifting_i,
      .tetrahedron_i = tetr_i,
      .weights       = bary 
    };

    // Find element indices for this tetrahedron, and then fill per-vertex data
    for (auto [i, elem_i] : enumerate_view(tessellation.elems[tetr_i])) {
      int j = static_cast<int>(elem_i) - static_cast<int>(uplifting_data.boundary.size());

      // Assign constraint index, or -1 if a constraint is a boundary vertex (meaning we're not interested)
      tr.indices[i] = std::max<int>(j, -1);                

      // Assign corresponding spectrum
      tr.spectra[i] = uplifting_data.boundary_and_interior[elem_i].spec; 
    }
      
    return tr;
  }

  std::vector<UpliftingInfo> query_uplifting_info(const Scene &scene, const PathRecord &rc) {
    met_trace();
    return rc.data | vws::take(std::max(static_cast<int>(rc.path_depth) - 1, 0)) 
                   | vws::transform([&](const auto &vt) { return query_uplifting_info(scene, vt); })
                   | view_to<std::vector<UpliftingInfo>>();
  }

  std::vector<ReconstructionInfo> query_path_reconstruction(const Scene &scene, const PathRecord &path, const ConstraintRecord &cs) {
    met_trace();

    // 1. Query uplifting info per path vertex; drop all info objects that operate on an irrelevant uplifting object
    auto upliftings 
      = query_uplifting_info(scene, path)
      | vws::filter([&](const auto &v) { return v.uplifting_i == cs.uplifting_i; })
      | view_to<std::vector<UpliftingInfo>>();

    // 2. Per info, identify the index of the vertex that is attached to our constraint reflectance
    //    and return both info and index, but filter out those that don't connect to our constraint
    auto vertex_i 
      = upliftings
      | vws::transform([&](const auto &v) { 
        uint i = std::distance(v.indices.begin(), rng::find(v.indices, cs.vertex_i)); 
        return std::pair { v, i };
      })
      | vws::filter([](const auto &pair) { return pair.second < 4; })
      | view_to<std::vector<std::pair<UpliftingInfo, uint>>>();

    // 3. Transform to compact representation of barycentric weight and remainder
    auto reconstructions 
      = vertex_i
      | vws::transform([&](const auto &pair) {
        // We now have four vertex weights, and the index of the constraint-associated vertex
        auto [v, i] = pair;
        ReconstructionInfo ri = { .a = v.weights[i], .remainder = 0.f };

        // For each vertex that isn't the constraint, we add their spectrum into the remainder
        for (uint j = 0; j < 4; ++j) {
          guard_continue(j != i);
          ri.remainder += v.weights[j] * sample_spectrum(path.wvls, v.spectra[j]);
        }

        return ri;
      })
      | view_to<std::vector<ReconstructionInfo>>();
    
    return reconstructions;
  }
} // namespace met::detail