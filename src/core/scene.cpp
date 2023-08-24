#include <metameric/core/scene.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/utility.hpp>
#include <nlohmann/json.hpp>
#include <zstr.hpp>
#include <iostream>

/* 
  std::variant serialization helper for json
  src: https://github.com/nlohmann/json/issues/1261#issuecomment-426200060
*/
namespace nlohmann {
  template <typename ...Args>
  struct adl_serializer<std::variant<Args...>> {
    static void to_json(json& j, std::variant<Args...> const& v) {
      std::visit([&](auto&& value) {
        j = std::forward<decltype(value)>(value);
      }, v);
    }
  };
}

namespace met {
  template <typename Ty>
  void to_stream(std::ostream &os, const Ty &ty) {
    met_trace();
    os.write((const char *) &ty, sizeof(Ty));
  }

  template <typename Ty>
  void from_stream(std::istream &is, Ty &ty) {
    met_trace();
    is.read((char *) &ty, sizeof(Ty));
  }

  template <typename Ty>
  void to_stream(std::ostream &os, const std::vector<Ty> &v) {
    met_trace();
    size_t size = v.size();
    to_stream(os, size);
    os.write((const char *) v.data(), size * sizeof(Ty));
  }

  template <typename Ty>
  void from_stream(std::istream &is, std::vector<Ty> &v) {
    met_trace();
    size_t size;
    from_stream(is, size);
    v.resize(size);
    is.read((char *) v.data(), size * sizeof(Ty));
  }

  template <typename Ty, uint D>
  void to_stream(std::ostream &os, const TextureBlock<Ty, D> &texture) {
    met_trace();
    auto size = texture.size();
    to_stream(os, size);
    os.write((const char *) texture.data(), size.prod() * sizeof(Ty));
  }

  template <typename Ty, uint D>
  void from_stream(std::istream &is, TextureBlock<Ty, D> &texture) {
    met_trace();
    auto size = texture.size();
    from_stream(is, size);
    texture = {{ .size = size }};
    is.read((char *) texture.data(), size.prod() * sizeof(Ty));
  }

  template <typename Vt, typename El>
  void to_stream(std::ostream &os, const MeshDataBase<Vt, El> &mesh) {
    met_trace();
    to_stream(os, mesh.verts);
    to_stream(os, mesh.elems);
    to_stream(os, mesh.norms);
    to_stream(os, mesh.uvs);
  }

  template <typename Vt, typename El>
  void from_stream(std::istream &is, MeshDataBase<Vt, El> &mesh) {
    met_trace();
    from_stream(is, mesh.verts);
    from_stream(is, mesh.elems);
    from_stream(is, mesh.norms);
    from_stream(is, mesh.uvs);
  }

  void to_stream(std::ostream &os, const Scene &scene) {
    met_trace();
    to_stream(os, scene.meshes);
    to_stream(os, scene.textures_3f);
    to_stream(os, scene.textures_1f);
    to_stream(os, scene.illuminants);
    to_stream(os, scene.observers);
    to_stream(os, scene.bases);
  }

  void from_stream(std::istream &is, Scene &scene) {
    met_trace();
    from_stream(is, scene.meshes);
    from_stream(is, scene.textures_3f);
    from_stream(is, scene.textures_1f);
    from_stream(is, scene.illuminants);
    from_stream(is, scene.observers);
    from_stream(is, scene.bases);
  }

  void to_json(json &js, const Uplifting::Constraint &cstr) {
    met_trace();
    js = {{ "type",   cstr.type   },
          { "colr_i", cstr.colr_i },
          { "csys_i", cstr.csys_i },
          { "colr_j", cstr.colr_j },
          { "csys_j", cstr.csys_j },
          { "spec",   cstr.spec   }};
  }

  void from_json(const json &js, Uplifting::Constraint &cstr) {
    met_trace();
    js.at("type").get_to(cstr.type);
    js.at("colr_i").get_to(cstr.colr_i);
    js.at("csys_i").get_to(cstr.csys_i);
    js.at("colr_j").get_to(cstr.colr_j);
    js.at("csys_j").get_to(cstr.csys_j);
    js.at("spec").get_to(cstr.spec);
  }

  void to_json(json &js, const Uplifting &uplifting) {
    met_trace();
    js = {{ "type",    uplifting.type    },
          { "basis_i", uplifting.basis_i },
          { "verts",   uplifting.verts   },
          { "elems",   uplifting.elems   }};
  }

  void from_json(const json &js, Uplifting &uplifting) {
    met_trace();
    js.at("type").get_to(uplifting.type);
    js.at("basis_i").get_to(uplifting.basis_i);
    js.at("verts").get_to(uplifting.verts);
    js.at("elems").get_to(uplifting.elems);
  }

  void to_json(json &js, const Scene::Object &object) {
    met_trace();
    js = {{ "mesh_i",      object.mesh_i      },
          { "material_i",  object.material_i  },
          { "uplifting_i", object.uplifting_i },
          { "trf",         object.trf         }};
  }

  void from_json(const json &js, Scene::Object &object) {
    met_trace();
    js.at("mesh_i").get_to(object.mesh_i);
    js.at("material_i").get_to(object.material_i);
    js.at("uplifting_i").get_to(object.uplifting_i);
    js.at("trf").get_to(object.trf);
  }

  void to_json(json &js, const Scene::Material &material) {
    met_trace();
    js["diffuse"]   = {{ "index", material.diffuse.index() },   { "variant", material.diffuse }};
    js["roughness"] = {{ "index", material.roughness.index() }, { "variant", material.roughness }};
    js["metallic"]  = {{ "index", material.metallic.index() },  { "variant", material.metallic }};
    js["opacity"]   = {{ "index", material.opacity.index() },   { "variant", material.opacity }};
  }

  void from_json(const json &js, Scene::Material &material) {
    met_trace();
    switch (js.at("diffuse").at("index").get<size_t>()) {
      case 0: material.diffuse = js.at("diffuse").at("variant").get<Colr>(); break;
      case 1: material.diffuse = js.at("diffuse").at("variant").get<uint>(); break;
      default: debug::check_expr(false, "Error parsing json material data");
    }
    switch (js.at("roughness").at("index").get<size_t>()) {
      case 0: material.roughness = js.at("roughness").at("variant").get<float>(); break;
      case 1: material.roughness = js.at("roughness").at("variant").get<uint>(); break;
      default: debug::check_expr(false, "Error parsing json material data");
    }
    switch (js.at("metallic").at("index").get<size_t>()) {
      case 0: material.metallic = js.at("metallic").at("variant").get<float>(); break;
      case 1: material.metallic = js.at("metallic").at("variant").get<uint>(); break;
      default: debug::check_expr(false, "Error parsing json material data");
    }
    switch (js.at("opacity").at("index").get<size_t>()) {
      case 0: material.opacity = js.at("opacity").at("variant").get<float>(); break;
      case 1: material.opacity = js.at("opacity").at("variant").get<uint>(); break;
      default: debug::check_expr(false, "Error parsing json material data");
    }
  }

  void to_json(json &js, const Scene::Emitter &emitter) {
    met_trace();
    js = {{ "p",            emitter.p            },
          { "multiplier",   emitter.multiplier   },
          { "illuminant_i", emitter.illuminant_i }};
  }

  void from_json(const json &js, Scene::Emitter &emitter) {
    met_trace();
    js.at("p").get_to(emitter.p);
    js.at("multiplier").get_to(emitter.multiplier);
    js.at("illuminant_i").get_to(emitter.illuminant_i);
  }

  void to_json(json &js, const Scene::ColrSystem &csys) {
    met_trace();
    js = {{ "observer_i",   csys.observer_i       },
          { "illuminant_i", csys.illuminant_i },
          { "n_scatters",   csys.n_scatters   }};
  }

  void from_json(const json &js, Scene::ColrSystem &csys) {
    met_trace();
    js.at("observer_i").get_to(csys.observer_i);
    js.at("illuminant_i").get_to(csys.illuminant_i);
    js.at("n_scatters").get_to(csys.n_scatters);
  }

  template <typename Ty, typename State>
  void to_json(json &js, const Scene::Component<Ty, State> &component) {
    met_trace();
    js = {{ "is_active", component.is_active },
          { "name",      component.name      },
          { "data",      component.data      }};
  }

  template <typename Ty, typename State>
  void from_json(const json &js, Scene::Component<Ty, State> &component) {
    met_trace();
    js.at("is_active").get_to(component.is_active);
    js.at("name").get_to(component.name);
    js.at("data").get_to(component.data);
  }

  template <typename Ty>
  void to_json(json &js, const Scene::Resource<Ty> &resource) {
    met_trace();
    js = {{ "name", resource.name   },
          { "path", resource.path   },
          { "data", resource.data() }};
  }

  template <typename Ty>
  void from_json(const json &js, Scene::Resource<Ty> &resource) {
    met_trace();
    js.at("name").get_to(resource.name);
    js.at("path").get_to(resource.path);
    js.at("data").get_to(resource.data());
  }

  void to_json(json &js, const Scene &scene) {
    met_trace();
    js = {{ "observer_i",    scene.observer_i },
          { "objects",       scene.objects    },
          { "emitters",      scene.emitters   },
          { "materials",     scene.materials  },
          { "upliftings",    scene.upliftings },
          { "colr_systems", scene.colr_systems }};
  }

  void from_json(const json &js, Scene &scene) {
    met_trace();
    js.at("observer_i").get_to(scene.observer_i);
    js.at("objects").get_to(scene.objects);
    js.at("emitters").get_to(scene.emitters);
    js.at("materials").get_to(scene.materials);
    js.at("upliftings").get_to(scene.upliftings);
    js.at("colr_systems").get_to(scene.colr_systems);
  }

  met::ColrSystem Scene::get_csys(uint i) const {
    met_trace();
    return get_csys(colr_systems[i].data);
  }

  met::ColrSystem Scene::get_csys(ColrSystem c) const {
    met_trace();
    return { .cmfs       = observers[c.observer_i].data(),
             .illuminant = illuminants[c.illuminant_i].data(),
             .n_scatters = c.n_scatters };
  }

  met::Spec Scene::get_emitter_spd(uint i) const {
    met_trace();
    return get_emitter_spd(emitters[i].data);
  }

  met::Spec Scene::get_emitter_spd(Emitter e) const {
    met_trace();
    return (illuminants[e.illuminant_i].data() * e.multiplier).eval();
  }

  std::string Scene::get_csys_name(uint i) const {
    met_trace();
    return get_csys_name(colr_systems[i].data);
  }

  std::string Scene::get_csys_name(ColrSystem c) const {
    met_trace();
    return std::format("{}, {}", 
                       observers[c.observer_i].name, 
                       illuminants[c.illuminant_i].name);
  }

  namespace io {
    constexpr auto scene_i_flags = std::ios::in  | std::ios::binary;
    constexpr auto scene_o_flags = std::ios::out | std::ios::binary | std::ios::trunc;

    Scene load_scene(const fs::path &path) {
      met_trace();

      // Get paths to .json and .data files with matching extensions
      fs::path json_path = path_with_ext(path, ".json");
      fs::path data_path = path_with_ext(path, ".data");

      Scene scene;

      // Attempt load and deserialize of .json file to initial scene object
      json js = load_json(json_path);
      js.get_to(scene);

      // Next, attempt opening zlib compressed stream, and deserialize to scene object
      auto ifs = zstr::ifstream(data_path.string(), scene_i_flags);
      from_stream(ifs, scene);
      
      return scene;
    }

    void save_scene(const fs::path &path, const Scene &scene) {
      met_trace();

      // Get paths to .json and .data files with matching extensions
      fs::path json_path = path_with_ext(path, ".json");
      fs::path data_path = path_with_ext(path, ".data");
      
      // Attempt opening zlib compressed stream, and serialize scene object
      auto ofs = zstr::ofstream(data_path.string(), scene_o_flags, -1);
      to_stream(ofs, scene);

      // Attempt serialize and save of scene object to .json file
      json js = scene;
      save_json(json_path, js);
    }
  } // namespace io
} // namespace met