#include <metameric/core/data_.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/utility.hpp>
#include <zstr.hpp>

namespace met {
  namespace io {
    namespace detail {
      template <typename T>
      void serialize_vector(const std::vector<T> &v,
                            zstr::ofstream       &ofs) {
        size_t size = v.size();
        ofs.write((const char *) &size, sizeof(size_t));
        ofs.write((const char *) v.data(), size * sizeof(T));
      }

      template <typename T>
      std::vector<T> deserialize_vector(zstr::ifstream &ifs) {
        size_t size;
        ifs.read((char *) &size, sizeof(size_t));
        std::vector<T> v(size);
        ifs.read((const char *) v.data(), size * sizeof(T));
        return v;   
      }

      void serialize_mesh(const Scene::Mesh &mesh,
                          zstr::ofstream    &ofs) {
        met_trace();
        serialize_vector(mesh.verts, ofs);
        serialize_vector(mesh.elems, ofs);
        serialize_vector(mesh.norms, ofs);
        serialize_vector(mesh.uvs,   ofs);
      }

      Scene::Mesh deserialize_mesh(zstr::ifstream &ifs) {
        met_trace();
        return Scene::Mesh {
          .verts = deserialize_vector<Scene::Mesh::VertTy>(ifs),
          .elems = deserialize_vector<Scene::Mesh::ElemTy>(ifs),
          .norms = deserialize_vector<Scene::Mesh::VertTy>(ifs),
          .uvs   = deserialize_vector<Scene::Mesh::UVTy>(ifs)
        };
      }
    }

    Scene load_scene(const fs::path &path) {
      met_trace();

      Scene scene;

      // Load spectral and scene objects from JSON

      // Load data objects from secondary pack file

      return scene;
    }

    void save_scene(const fs::path &path, const Scene &scene) {
      met_trace();
      
      // Save data objects to secondary pack file

      // Save spectral and scene objects to JSON
    }
  } // namespace io
} // namespace met