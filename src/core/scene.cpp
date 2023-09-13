#include <metameric/core/scene.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/serialization.hpp>
#include <metameric/core/utility.hpp>
#include <nlohmann/json.hpp>
#include <zstr.hpp>

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
  namespace detail {
    template <typename Ty, typename State>
    void to_json(json &js, const detail::Component<Ty, State> &component) {
      met_trace();
      js = {{ "name",      component.name  },
            { "value",     component.value }};
    }

    template <typename Ty, typename State>
    void from_json(const json &js, detail::Component<Ty, State> &component) {
      met_trace();
      js.at("name").get_to(component.name);
      js.at("value").get_to(component.value);
    }
  } // namespace detail

  void to_json(json &js, const Uplifting::Constraint &cstr) {
    met_trace();
    js = {{ "type",             cstr.type             },
          { "colr_i",           cstr.colr_i           },
          { "csys_i",           cstr.csys_i           },
          { "colr_j",           cstr.colr_j           },
          { "csys_j",           cstr.csys_j           },
          { "object_i",         cstr.object_i         },
          { "object_elem_i",    cstr.object_elem_i    },
          { "object_elem_bary", cstr.object_elem_bary },
          { "measurement",      cstr.measurement      }};
  }

  void from_json(const json &js, Uplifting::Constraint &cstr) {
    met_trace();
    js.at("type").get_to(cstr.type);
    js.at("colr_i").get_to(cstr.colr_i);
    js.at("csys_i").get_to(cstr.csys_i);
    js.at("colr_j").get_to(cstr.colr_j);
    js.at("csys_j").get_to(cstr.csys_j);
    js.at("object_i").get_to(cstr.object_i);
    js.at("object_elem_i").get_to(cstr.object_elem_i);
    js.at("object_elem_bary").get_to(cstr.object_elem_bary);
    js.at("measurement").get_to(cstr.measurement);
  }

  void to_json(json &js, const Uplifting &uplifting) {
    met_trace();
    js = {{ "type",    uplifting.type    },
          { "csys_i",  uplifting.csys_i },
          { "basis_i", uplifting.basis_i },
          { "verts",   uplifting.verts   },
          { "elems",   uplifting.elems   }};
  }

  void from_json(const json &js, Uplifting &uplifting) {
    met_trace();
    js.at("type").get_to(uplifting.type);
    js.at("csys_i").get_to(uplifting.csys_i);
    js.at("basis_i").get_to(uplifting.basis_i);
    js.at("verts").get_to(uplifting.verts);
    js.at("elems").get_to(uplifting.elems);
  }

  void to_json(json &js, const Scene::Object &object) {
    met_trace();
    js = {{ "is_active",   object.is_active   },
          { "mesh_i",      object.mesh_i      },
          { "uplifting_i", object.uplifting_i },
          { "trf",         object.trf         }};
    js["diffuse"]   = {{ "index", object.diffuse.index() },   { "variant", object.diffuse }};
    js["roughness"] = {{ "index", object.roughness.index() }, { "variant", object.roughness }};
    js["metallic"]  = {{ "index", object.metallic.index() },  { "variant", object.metallic }};
    js["opacity"]   = {{ "index", object.opacity.index() },   { "variant", object.opacity }};
    js["normals"]   = {{ "index", object.normals.index() },   { "variant", object.normals }};
  }

  void from_json(const json &js, Scene::Object &object) {
    met_trace();
    js.at("is_active").get_to(object.is_active);
    js.at("mesh_i").get_to(object.mesh_i);
    js.at("uplifting_i").get_to(object.uplifting_i);
    js.at("trf").get_to(object.trf);
    switch (js.at("diffuse").at("index").get<size_t>()) {
      case 0: object.diffuse = js.at("diffuse").at("variant").get<Colr>(); break;
      case 1: object.diffuse = js.at("diffuse").at("variant").get<uint>(); break;
      default: debug::check_expr(false, "Error parsing json material data");
    }
    switch (js.at("roughness").at("index").get<size_t>()) {
      case 0: object.roughness = js.at("roughness").at("variant").get<float>(); break;
      case 1: object.roughness = js.at("roughness").at("variant").get<uint>(); break;
      default: debug::check_expr(false, "Error parsing json material data");
    }
    switch (js.at("metallic").at("index").get<size_t>()) {
      case 0: object.metallic = js.at("metallic").at("variant").get<float>(); break;
      case 1: object.metallic = js.at("metallic").at("variant").get<uint>(); break;
      default: debug::check_expr(false, "Error parsing json material data");
    }
    switch (js.at("opacity").at("index").get<size_t>()) {
      case 0: object.opacity = js.at("opacity").at("variant").get<float>(); break;
      case 1: object.opacity = js.at("opacity").at("variant").get<uint>(); break;
      default: debug::check_expr(false, "Error parsing json material data");
    }
    switch (js.at("normals").at("index").get<size_t>()) {
      case 0: object.normals = js.at("normals").at("variant").get<Colr>(); break;
      case 1: object.normals = js.at("normals").at("variant").get<uint>(); break;
      default: debug::check_expr(false, "Error parsing json material data");
    }
  }

  void to_json(json &js, const Scene::Emitter &emitter) {
    met_trace();
    js = {{ "is_active",    emitter.is_active   },
          { "p",            emitter.p            },
          { "multiplier",   emitter.multiplier   },
          { "illuminant_i", emitter.illuminant_i }};
  }

  void from_json(const json &js, Scene::Emitter &emitter) {
    met_trace();
    js.at("is_active").get_to(emitter.is_active);
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

  void to_json(json &js, const Scene &scene) {
    met_trace();
    js = {{ "observer_i",    scene.observer_i          },
          { "objects",       scene.components.objects.data()      },
          { "emitters",      scene.components.emitters.data()     },
          { "upliftings",    scene.components.upliftings.data()   },
          { "colr_systems",  scene.components.colr_systems.data() }};
  }

  void from_json(const json &js, Scene &scene) {
    met_trace();
    js.at("observer_i").get_to(scene.observer_i);
    js.at("objects").get_to(scene.components.objects.data());
    js.at("emitters").get_to(scene.components.emitters.data());
    js.at("upliftings").get_to(scene.components.upliftings.data());
    js.at("colr_systems").get_to(scene.components.colr_systems.data());
  }

  met::ColrSystem Scene::get_csys(uint i) const {
    met_trace();
    return get_csys(components.colr_systems[i].value);
  }

  met::ColrSystem Scene::get_csys(ColrSystem c) const {
    met_trace();
    return { .cmfs       = resources.observers[c.observer_i].value(),
             .illuminant = resources.illuminants[c.illuminant_i].value(),
             .n_scatters = c.n_scatters };
  }

  met::Spec Scene::get_emitter_spd(uint i) const {
    met_trace();
    return get_emitter_spd(components.emitters[i].value);
  }

  met::Spec Scene::get_emitter_spd(Emitter e) const {
    met_trace();
    return (resources.illuminants[e.illuminant_i].value() * e.multiplier).eval();
  }

  std::string Scene::get_csys_name(uint i) const {
    met_trace();
    return get_csys_name(components.colr_systems[i].value);
  }

  std::string Scene::get_csys_name(ColrSystem c) const {
    met_trace();
    return std::format("{}, {}", 
                       resources.observers[c.observer_i].name, 
                       resources.illuminants[c.illuminant_i].name);
  }

  void Scene::to_stream(std::ostream &str) const {
    met_trace();
    io::to_stream(resources.meshes,      str);
    io::to_stream(resources.images,      str);
    io::to_stream(resources.illuminants, str);
    io::to_stream(resources.observers,   str);
    io::to_stream(resources.bases,       str);
  }

  void Scene::fr_stream(std::istream &str) {
    met_trace();
    io::fr_stream(resources.meshes,      str);
    io::fr_stream(resources.images,      str);
    io::fr_stream(resources.illuminants, str);
    io::fr_stream(resources.observers,   str);
    io::fr_stream(resources.bases,       str);
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
      auto str = zstr::ifstream(data_path.string(), scene_i_flags);
      debug::check_expr(str.good());
      scene.fr_stream(str);
      
      return scene;
    }

    void save_scene(const fs::path &path, const Scene &scene) {
      met_trace();

      // Get paths to .json and .data files with matching extensions
      fs::path json_path = path_with_ext(path, ".json");
      fs::path data_path = path_with_ext(path, ".data");
      
      // Attempt opening zlib compressed stream, and serialize scene resources
      auto str = zstr::ofstream(data_path.string(), scene_o_flags, Z_BEST_SPEED);
      debug::check_expr(str.good());
      scene.to_stream(str);

      // Attempt serialize and save of scene object to .json file
      json js = scene;
      save_json(json_path, js);
    }
  } // namespace io
} // namespace met