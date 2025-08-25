// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <metameric/scene/scene.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/serialization.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/packing.hpp>
#include <nlohmann/json.hpp>
#include <rapidobj/rapidobj.hpp>
#include <zstr.hpp>
#include <algorithm>
#include <execution>
#include <numbers>
#include <queue>
#include <unordered_map>

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
  constexpr auto scene_i_flags = std::ios::in  | std::ios::binary;
  constexpr auto scene_o_flags = std::ios::out | std::ios::binary | std::ios::trunc;

  namespace detail {
    // Write component name and underlying data to json
    template <typename Ty>
    void to_json(json &js, const detail::Component<Ty> &component) {
      met_trace();
      js = {{ "name",  component.name  },
            { "value", component.value }};
    }

    // Read component name and underlying data from json
    template <typename Ty>
    void from_json(const json &js, detail::Component<Ty> &component) {
      met_trace();
      js.at("name").get_to(component.name);
      js.at("value").get_to(component.value);
    }
  } // namespace detail

  void to_json(json &js, const Uplifting::Vertex &vert) {
    met_trace();
    js = {{ "name",      vert.name               },
          { "is_active", vert.is_active          },
          { "index",     vert.constraint.index() }, 
          { "variant",   vert.constraint         }};
  }

  void from_json(const json &js, Uplifting::Vertex &vert) {
    met_trace();
    js.at("name").get_to(vert.name);
    js.at("is_active").get_to(vert.is_active);
    switch (js.at("index").get<size_t>()) {
      case 0: vert.constraint = js.at("variant").get<MeasurementConstraint>(); break;
      case 1: vert.constraint = js.at("variant").get<DirectColorConstraint>(); break;
      case 2: vert.constraint = js.at("variant").get<DirectSurfaceConstraint>(); break;
      case 3: vert.constraint = js.at("variant").get<IndirectSurfaceConstraint>(); break;
      default: debug::check_expr(false, "Error parsing json constraint data");
    }
  }

  void to_json(json &js, const Uplifting &uplifting) {
    met_trace();
    js = {{ "observer_i",   uplifting.observer_i   },
          { "illuminant_i", uplifting.illuminant_i },
          { "basis_i",      uplifting.basis_i      },
          { "verts",        uplifting.verts        }};
  }

  void from_json(const json &js, Uplifting &uplifting) {
    met_trace();
    if (js.contains("csys_i")) {
      // Legacy code; set to default for now
      uplifting.observer_i = 0;
      uplifting.illuminant_i = 0;
    } else {
      js.at("observer_i").get_to(uplifting.observer_i);
      js.at("illuminant_i").get_to(uplifting.illuminant_i);
    }
    js.at("basis_i").get_to(uplifting.basis_i);
    js.at("verts").get_to(uplifting.verts);
  }

  void to_json(json &js, const Settings &settings) {
    met_trace();
    js = {{ "renderer_type", settings.renderer_type },
          { "texture_size",  settings.texture_size  },
          { "view_i",        settings.view_i        },
          { "view_scale",    settings.view_scale    }};
  }

  void from_json(const json &js, Settings &settings) {
    met_trace();
    js.at("renderer_type").get_to(settings.renderer_type);
    js.at("texture_size").get_to(settings.texture_size);
    js.at("view_i").get_to(settings.view_i);
    js.at("view_scale").get_to(settings.view_scale);
  }

  void to_json(json &js, const View &view) {
    met_trace();
    js = {{ "draw_frustrum", view.draw_frustrum },
          { "observer_i",    view.observer_i    },
          { "camera_trf",    view.camera_trf    },
          { "camera_fov_y",  view.camera_fov_y  },
          { "film_size",     view.film_size     }};
  }

  void from_json(const json &js, View &view) {
    met_trace();
    if (js.contains("draw_frustrum"))
      js.at("draw_frustrum").get_to(view.draw_frustrum);
    js.at("observer_i").get_to(view.observer_i);
    js.at("camera_trf").get_to(view.camera_trf);
    js.at("camera_fov_y").get_to(view.camera_fov_y);
    js.at("film_size").get_to(view.film_size);
  }

  void to_json(json &js, const Object &object) {
    met_trace();
    js = {{ "is_active",   object.is_active   },
          { "transform",   object.transform   },
          { "mesh_i",      object.mesh_i      },
          { "uplifting_i", object.uplifting_i },
          { "brdf_type",   object.brdf_type   },
          { "eta_minmax",  object.eta_minmax  },
          { "absorption",  object.absorption  }};
    js["diffuse"]   = {{ "index", object.diffuse.index() },   { "variant", object.diffuse   }};
    js["roughness"] = {{ "index", object.roughness.index() }, { "variant", object.roughness }};
    js["metallic"]  = {{ "index", object.metallic.index() },  { "variant", object.metallic  }};
    js["normalmap"] = {{ "has_value", object.normalmap.has_value() },  { "value", object.normalmap.value_or(0u) }};
  }

  void from_json(const json &js, Object &object) {
    met_trace();
    js.at("is_active").get_to(object.is_active);
    js.at("transform").get_to(object.transform);
    js.at("mesh_i").get_to(object.mesh_i);
    js.at("uplifting_i").get_to(object.uplifting_i);
    if (!js.contains("brdf_type")) {
      object.brdf_type = Object::BRDFType::eDiffuse;
    } else {
      js.at("brdf_type").get_to(object.brdf_type);
    }
    switch (js.at("diffuse").at("index").get<size_t>()) {
      case 0: object.diffuse = js.at("diffuse").at("variant").get<Colr>(); break;
      case 1: object.diffuse = js.at("diffuse").at("variant").get<uint>(); break;
      default: debug::check_expr(false, "Error parsing json material data");
    }
    if (js.contains("metallic")) {
      switch (js.at("metallic").at("index").get<size_t>()) {
        case 0: object.metallic = js.at("metallic").at("variant").get<float>(); break;
        case 1: object.metallic = js.at("metallic").at("variant").get<uint>(); break;
        default: debug::check_expr(false, "Error parsing json material data");
      }
    }
    if (js.contains("roughness")) {
      switch (js.at("roughness").at("index").get<size_t>()) {
        case 0: object.roughness = js.at("roughness").at("variant").get<float>(); break;
        case 1: object.roughness = js.at("roughness").at("variant").get<uint>(); break;
        default: debug::check_expr(false, "Error parsing json material data");
      }
    }
    if (js.contains("eta_minmax")) {
      js.at("eta_minmax").get_to(object.eta_minmax);
    }
    if (js.contains("absorption")) {
      js.at("absorption").get_to(object.absorption);
    }
    if (js.contains("normalmap") && js.at("normalmap").at("has_value").get<bool>()) {
      object.normalmap = js.at("normalmap").at("value").get<uint>();
    }
  }

  void to_json(json &js, const Emitter &emitter) {
    met_trace();
    js = {{ "type",             emitter.type             },
          { "spec_type",        emitter.spec_type        },
          { "transform",        emitter.transform        },
          { "is_active",        emitter.is_active        },
          { "illuminant_i",     emitter.illuminant_i     },
          { "illuminant_scale", emitter.illuminant_scale }};
    js["color"] = {{ "index", emitter.color.index() }, { "variant", emitter.color }};
  }

  void from_json(const json &js, Emitter &emitter) {
    met_trace();
    js.at("type").get_to(emitter.type);
    js.at("is_active").get_to(emitter.is_active);
    js.at("transform").get_to(emitter.transform);
    js.at("illuminant_i").get_to(emitter.illuminant_i);
    js.at("illuminant_scale").get_to(emitter.illuminant_scale);
    if (js.contains("spec_type")) {
      js.at("spec_type").get_to(emitter.spec_type);
    }
    if (js.contains("color")) {
      switch (js.at("color").at("index").get<size_t>()) {
        case 0: emitter.color = js.at("color").at("variant").get<Colr>(); break;
        case 1: emitter.color = js.at("color").at("variant").get<uint>(); break;
        default: debug::check_expr(false, "Error parsing json material data");
      }
    }
  }

  void to_json(json &js, const Scene &scene) {
    met_trace();
    js = {{ "settings",      scene.components.settings            },
          { "objects",       scene.components.objects.data()      },
          { "emitters",      scene.components.emitters.data()     },
          { "upliftings",    scene.components.upliftings.data()   },
          { "views",         scene.components.views.data()        }};
  }

  void from_json(const json &js, Scene &scene) {
    met_trace();
    js.at("settings").get_to(scene.components.settings);
    js.at("objects").get_to(scene.components.objects.data());
    js.at("emitters").get_to(scene.components.emitters.data());
    js.at("upliftings").get_to(scene.components.upliftings.data());
    js.at("views").get_to(scene.components.views.data());
  }

  // Scene serialization to/from si partial; only resource data is serialized
  namespace io {
    void to_stream(const Scene &scene, std::ostream &str) {
      met_trace();
      io::to_stream(scene.resources.meshes,      str);
      io::to_stream(scene.resources.images,      str);
      io::to_stream(scene.resources.illuminants, str);
      io::to_stream(scene.resources.observers,   str);
      io::to_stream(scene.resources.bases,       str);
    }

    void from_stream(Scene &scene, std::istream &str) {
      met_trace();
      io::from_stream(scene.resources.meshes,      str);
      io::from_stream(scene.resources.images,      str);
      io::from_stream(scene.resources.illuminants, str);
      io::from_stream(scene.resources.observers,   str);
      io::from_stream(scene.resources.bases,       str);
    }
  } // namespace io

  Scene::Scene(ResourceHandle cache_handle) 
  : m_cache_handle(cache_handle) { }
  
  void Scene::create() {
    met_trace();
 
    // Clear out scene first
    unload();

    // Normalized D65 integrating to 1 luminance; probably more useful during rendering
    // than regular D65
    Spec d65n;
    {
      ColrSystem csys = { .cmfs = models::cmfs_cie_xyz, .illuminant = Spec(1) };
      d65n = models::emitter_cie_d65 / luminance(csys(models::emitter_cie_d65));
    }
  
    // Pre-supply some data for an initial scene
    components.settings = { .name  = "Settings", .value = {  }};
    resources.illuminants.push("D65",              models::emitter_cie_d65,     false);
    resources.illuminants.push("D65 (normalized)", d65n,                        false);
    resources.illuminants.push("E",                models::emitter_cie_e,       false);
    resources.illuminants.push("FL2",              models::emitter_cie_fl2,     false);
    resources.illuminants.push("FL11",             models::emitter_cie_fl11,    false);
    resources.illuminants.push("LED-RGB1",         models::emitter_cie_ledrgb1, false);
    resources.illuminants.push("LED-B1",           models::emitter_cie_ledb1,   false);
    resources.observers.push("CIE XYZ",            models::cmfs_cie_xyz,        false);
    resources.meshes.push("Rectangle",             models::unit_rect,           false);

    auto basis = io::load_basis("data/basis_262144.txt");
    for (auto col : basis.func.colwise()) {
      auto min_coeff = col.minCoeff(), max_coeff = col.maxCoeff();
      col /= std::max(std::abs(max_coeff), std::abs(min_coeff));
    }
    resources.bases.push("Default basis", basis, false);

    // Default view, used by viewport
    components.views.push("Default view", View());
    
    // Default plane
    components.objects.emplace("Default object", { 
      .transform   = { .position = { 0.f, 0.f, 0.f },
                       .rotation = { 0.f, -90.f * std::numbers::pi_v<float> / 180.f, 0.f },
                       .scaling  = 1.f },
      .mesh_i      = 0, 
      .uplifting_i = 0,
      .diffuse     = Colr(0.5) 
    });

    // Default uplifting
    components.upliftings.emplace("Default uplifting", { 
      .observer_i   = 0,
      .illuminant_i = 0,
      .basis_i      = 0 
    });

    // Default emitter
    components.emitters.push("Default emitter", {
      .type             = Emitter::Type::eRect,
      .transform        = { .position = { 0.f, 1.f, 0.f },
                            .rotation = { 0.f, 90.f * std::numbers::pi_v<float> / 180.f, 0.f },
                            .scaling  = .5f },
      .illuminant_i     = 1, // normalized d65
      .illuminant_scale = 1
    });
    
    // Set state to fresh create
    save_path  = "";
    save_state = SaveState::eNew;
    clear_mods();

    fmt::print("Scene: created scene\n");
  }

  void Scene::unload() {
    met_trace();

    // Clear out scene first
    *this = { m_cache_handle };

    // Set state to unloaded
    save_path  = "";
    save_state = SaveState::eUnloaded;
    clear_mods();
    
    fmt::print("Scene: unloaded scene\n");
  }

  void Scene::save(const fs::path &path) {
    met_trace();

    // Get paths to .json and .data files with matching extensions
    fs::path json_path = io::path_with_ext(path, ".json");
    fs::path data_path = io::path_with_ext(path, ".data");
      
    // Attempt opening zlib compressed stream, and serialize scene resources
    // auto str = zstr::ofstream(data_path.string(), scene_o_flags, Z_BEST_SPEED);
    auto str = std::ofstream(data_path.string(), scene_o_flags);
    debug::check_expr(str.good());
    io::to_stream(*this, str);

    // Attempt serialize and save of scene object to .json file
    json js = *this;
    io::save_json(json_path, js);

    // Set state to fresh save
    save_path  = io::path_with_ext(path, ".json");
    save_state = SaveState::eSaved;
    
    fmt::print("Scene: saved \"{}\"\n", path.string());
  }

  void Scene::load(const fs::path &path) {
    met_trace();
    
    // Clear out scene first
    unload();

    // Get paths to .json and .data files with matching extensions
    fs::path json_path = io::path_with_ext(path, ".json");
    fs::path data_path = io::path_with_ext(path, ".data");

    // Attempt load and deserialize of .json file to initial scene object
    json js = io::load_json(json_path);
    js.get_to(*this);

    // Next, attempt opening zlib compressed stream, and deserialize to scene object;
    // alternatively, open uncompressed stream and deserialize
    try {
      auto str = zstr::ifstream(data_path.string(), scene_i_flags);
      debug::check_expr(str.good());
      io::from_stream(*this, str);
    } catch (const std::exception &e) {
      fmt::print(stderr, "{}\n", e.what());
      auto str = std::ifstream(data_path.string(), scene_i_flags);
      io::from_stream(*this, str);
    }
      
    // Set state to fresh load
    save_path  = io::path_with_ext(path, ".json");
    save_state = SaveState::eSaved;
    clear_mods();

    fmt::print("Scene: loaded \"{}\"\n", path.string());
  }

  void Scene::import_obj(const fs::path obj_path, bool load_materials, bool flip_uvs) {
    met_trace();

    // Check that file path exists
    debug::check_expr(fs::exists(obj_path),
      fmt::format("failed to resolve path \"{}\"", obj_path.string()));

    // Attempt to parse OBJ file using rapidobj
    rapidobj::Result result = rapidobj::ParseFile(obj_path);
    debug::check_expr(!result.error,
      fmt::format("failed to parse obj file \"{}\" with error \"{}\"", obj_path.string(), result.error.code.message()));

    // Obtain triangulated result
    debug::check_expr(rapidobj::Triangulate(result),
      fmt::format("failed to triangulate obj file \"{}\" with error \"{}\"", obj_path.string(), result.error.code.message()));
    
    // Obtain data soup ranges; vertex color is discarded
    auto obj_verts = cnt_span<eig::Array3f>(result.attributes.positions);
    auto obj_norms = cnt_span<eig::Array3f>(result.attributes.normals);
    auto obj_txuvs = cnt_span<eig::Array2f>(result.attributes.texcoords);

    // Import object; create (empty) scene to store objects/meshes/textures for output
    Scene scene = { ResourceHandle() };

    // List of material textures to load, coupled
    // to a compact list of scene texture IDs
    std::unordered_map<std::string, uint> texture_load_list;

    // For each rapidobj shape, we attempt to 
    // 1 - create a mesh resource 
    // 2 - identify a referred texture resource or specify a single diffuse value
    // 3 - create an object component referring to mesh/texture
    // 4 - store mesh and object in scene
    // 5 - load referred textures and store them in scene
    for (const auto &shape : result.shapes) {
      // Skip non-polyhedral shapes
      guard_continue(!shape.mesh.indices.empty());

      bool has_norms = shape.mesh.indices.front().normal_index >= 0 && !obj_norms.empty();
      bool has_txuvs = shape.mesh.indices.front().texcoord_index >= 0 && !obj_txuvs.empty();
      bool has_matrs = !shape.mesh.material_ids.empty() && !result.materials.empty();
      
      // 1 - create a mesh resource
      met::Mesh mesh;
      {
        // First, allocate necessary vector sizes for duplicated mesh data; we deduplicate later
        mesh.verts.resize(shape.mesh.indices.size());
        if (has_norms)
          mesh.norms.resize(shape.mesh.indices.size());
        if (has_txuvs)
          mesh.txuvs.resize(shape.mesh.indices.size());
        mesh.elems.resize(shape.mesh.indices.size() / 3);

        // Then, fill mesh indices from 0 to n; we deduplicate later
        rng::iota(cnt_span<uint>(mesh.elems), 0u);

        // Next, copy vertex data from data soup to mesh; we deduplicate later
        #pragma omp parallel for
        for (int i = 0; i < shape.mesh.indices.size(); ++i) {
          const auto &obj_elem = shape.mesh.indices[i];
          mesh.verts[i] = obj_verts[obj_elem.position_index];
          if (has_norms)
            mesh.norms[i] = obj_norms[obj_elem.normal_index];
          if (has_txuvs)
            mesh.txuvs[i] = obj_txuvs[obj_elem.texcoord_index];
        } // for (i)

        // Optionally, flip UV coordinates along the y-axis
        if (flip_uvs)
          std::for_each(
            std::execution::par_unseq, 
            range_iter(mesh.txuvs),
            [](eig::Array2f &v) { v.y() = 1.f - v.y(); });

        // Finally, deduplicate and prepare for rendering
        remap_mesh(mesh);
        compact_mesh(mesh);
      }

      // 2 - Identify referred texture resource or specify a single value, and
      //     generally identify the brdf.
      std::variant<Colr, uint>  diffuse   = Colr(.5f);
      std::variant<float, uint> metallic  = 0.f;
      std::variant<float, uint> roughness = 1.f;
      std::optional<uint>       normalmap = { };
      Object::BRDFType brdf_type = Object::BRDFType::eDiffuse;
      if (load_materials && has_matrs) {
        // Access first material only; we ignore per-face materials; 
        const auto &obj_mat = result.materials[shape.mesh.material_ids.front()];
        
        if (obj_mat.diffuse_texname.empty()) {
          // Assign color value if there is no file path
          diffuse = Colr { obj_mat.diffuse[0], obj_mat.diffuse[1], obj_mat.diffuse[2] };
        } else {
          // Assign an allocated texture id from texture_load_list or get a new one
          diffuse = texture_load_list.insert({ 
            obj_mat.diffuse_texname,                    // filename of texture as key
            static_cast<uint>(texture_load_list.size()) // New texture id at end of list
          }).first->second;
        }

        if (obj_mat.metallic_texname.empty()) {
          metallic = obj_mat.metallic;
          if (obj_mat.roughness != 1.f || obj_mat.metallic != 0.f)
            brdf_type = Object::BRDFType::eMicrofacet;
        } else {
          // Assign an allocated texture id from texture_load_list or get a new one
          metallic = texture_load_list.insert({ 
            obj_mat.metallic_texname,                   // filename of texture as key
            static_cast<uint>(texture_load_list.size()) // New texture id at end of list
          }).first->second;
          brdf_type = Object::BRDFType::eMicrofacet;
        }
        
        if (obj_mat.roughness_texname.empty()) {
          roughness = obj_mat.roughness;
          if (obj_mat.roughness != 1.f || obj_mat.metallic != 0.f)
            brdf_type = Object::BRDFType::eMicrofacet;
        } else {
          // Assign an allocated texture id from texture_load_list or get a new one
          roughness = texture_load_list.insert({ 
            obj_mat.roughness_texname,                   // filename of texture as key
            static_cast<uint>(texture_load_list.size()) // New texture id at end of list
          }).first->second;
          brdf_type = Object::BRDFType::eMicrofacet;
        }

        if (!obj_mat.normal_texname.empty()) {
          normalmap = texture_load_list.insert({
            obj_mat.normal_texname,                   // filename of texture as key
            static_cast<uint>(texture_load_list.size()) // New texture id at end of list
          }).first->second;
        }
      }

      // 3 - create an object component referring to mesh/texture
      met::Object object = {
        .mesh_i      = static_cast<uint>(scene.resources.meshes.size()),
        .uplifting_i = 0,
        .brdf_type   = brdf_type,
        .diffuse     = diffuse,
        .metallic    = metallic,
        .roughness   = roughness,
        .eta_minmax  = { 1.25f, 1.25f },
        .normalmap   = normalmap
      };

      // 4 - store mesh and object in scene
      scene.resources.meshes.push(shape.name, std::move(mesh));
      scene.components.objects.push(shape.name, std::move(object));
    } // for (shape)

    // 5 - load required textures and store them in scene 
    for (const auto &[texture_path, i] : texture_load_list) {
      fs::path img_path = obj_path.parent_path() / texture_path;
      met::Image img = {{ .path = img_path }};
      scene.resources.images.push(img_path.filename().string(), std::move(img));
    }

    // Forward to scene importer
    import_scene(std::move(scene));
  }

  void Scene::import_scene(const fs::path &path) {
    met_trace();
    Scene other = { m_cache_handle };
    other.load(path);
    import_scene(std::move(other));

    fmt::print("Scene: imported \"{}\"\n", path.string());
  }

  void Scene::import_scene(Scene &&other) {
    // Import upliftings, and take care of index bookkeeping where necessary
    std::transform(range_iter(other.components.upliftings), 
      std::back_inserter(components.upliftings.data()), [&](auto component) {
      if (!other.resources.observers.empty())
        component->observer_i += resources.observers.size();
      if (!other.resources.illuminants.empty())
        component->illuminant_i += resources.illuminants.size();
      if (!other.resources.bases.empty())
        component->basis_i += resources.bases.size();
      return component;
    });

    // Import objects, and take care of index bookkeeping where necessary
    std::transform(range_iter(other.components.objects), 
      std::back_inserter(components.objects.data()), [&](auto component) {
      if (!other.resources.meshes.empty())
        component->mesh_i += resources.meshes.size();
      if (!other.components.upliftings.empty())
        component->uplifting_i += components.upliftings.size();
      if (component->diffuse.index() == 1)
        component->diffuse = static_cast<uint>(std::get<1>(component->diffuse) + resources.images.size());
      return component;
    });

    // Import emitters, and take care of index bookkeeping where necessary
    std::transform(range_iter(other.components.emitters), 
      std::back_inserter(components.emitters.data()), [&](auto component) {
      if (!other.resources.illuminants.empty())
        component->illuminant_i += resources.illuminants.size();
      return component;
    });

    // Import views, and take care of index bookkeeping where necessary
    std::transform(range_iter(other.components.views), 
      std::back_inserter(components.views.data()), [&](auto component) {
      if (!other.resources.observers.empty())
        component->observer_i += resources.observers.size();
      return component;
    });

    // Append scene resources from other scene behind current scene's components
    resources.meshes.data().insert(resources.meshes.end(),           
      std::make_move_iterator(other.resources.meshes.begin()),
      std::make_move_iterator(other.resources.meshes.end()));
    resources.images.data().insert(resources.images.end(),           
      std::make_move_iterator(other.resources.images.begin()),
      std::make_move_iterator(other.resources.images.end()));
    resources.illuminants.data().insert(resources.illuminants.end(), 
      std::make_move_iterator(other.resources.illuminants.begin()),
      std::make_move_iterator(other.resources.illuminants.end()));
    resources.observers.data().insert(resources.observers.end(),     
      std::make_move_iterator(other.resources.observers.begin()),
      std::make_move_iterator(other.resources.observers.end()));
    resources.bases.data().insert(resources.bases.end(),             
      std::make_move_iterator(other.resources.bases.begin()),
      std::make_move_iterator(other.resources.bases.end()));
  }

  void Scene::clean(Scene::CleanInfo &&info) {
    met_trace();

    if (info.delete_unused_meshes) {
      // Flag meshes that remain in use
      std::vector<uint> mask(resources.meshes.size(), 0);
      for (const auto &[object, _] : components.objects)
        mask[object.mesh_i] = 1;
      
      // Correct indices of remaining meshes using exclusive scan
      std::vector<uint> indices(mask.size());
      std::exclusive_scan(range_iter(mask), indices.begin(), 0u);
      for (auto &[object, _] : components.objects)
        object.mesh_i = indices[object.mesh_i];

      // Delete unused meshes
      uint n_deletes = 0;
      for (auto [i, flag] : enumerate_view(mask)) {
        guard_continue(flag == 0);
        resources.meshes.erase(i - n_deletes);
        n_deletes++;
      }
    }

    if (info.delete_unused_images) {
      // Flag images that remain in use by objects or emitters
      std::vector<uint> mask(resources.images.size(), 0);
      for (const auto &[object, _] : components.objects) {
        auto visitor = visit_single([&](uint i) { mask[i] = 1; });
        object.diffuse   | visitor;
        object.metallic  | visitor;
        object.roughness | visitor;
        object.normalmap | visitor;
      }
      for (const auto &[emitter, _] : components.emitters) {
        guard_continue(emitter.spec_type == Emitter::SpectrumType::eColr);
        auto visitor = visit_single([&](uint i) { mask[i] = 1; });
        emitter.color | visitor;
      }
      
      // Correct indices of remaining images using exclusive scan
      std::vector<uint> indices(mask.size());
      std::exclusive_scan(range_iter(mask), indices.begin(), 0u);
      for (auto &[object, _] : components.objects) {
        auto visitor = visit_single([&](uint &i) { i = indices[i]; });
        object.diffuse   | visitor;
        object.metallic  | visitor;
        object.roughness | visitor;
        object.normalmap | visitor;
      }
      for (auto &[emitter, _] : components.emitters) {
        guard_continue(emitter.spec_type == Emitter::SpectrumType::eColr);
        auto visitor = visit_single([&](uint &i) { i = indices[i]; });
        emitter.color | visitor;
      }

      // Delete unused images
      uint n_deletes = 0;
      for (auto [i, flag] : enumerate_view(mask)) {
        guard_continue(flag == 0);
        resources.images.erase(i - n_deletes);
        n_deletes++;
      }
    }

    if (info.delete_unused_observers) {
      /* ... */
    }

    if (info.delete_unused_illuminants) {
      /* ... */
    }

    if (info.delete_unused_bases) {
      /* ... */
    }
    
    fmt::print("Scene: cleaned scene\n");
  }

  void Scene::touch(Scene::SceneMod &&mod) {
    met_trace();

    // Apply change
    mod.redo(*this);

    // Ensure mod list doesn't exceed fixed length
    // and set the current mod to its end
    int n_mods = std::clamp(mod_i + 1, 0, 128);
    mod_i = n_mods;
    mods.resize(mod_i);
    mods.push_back(mod);   
    
    if (save_state == SaveState::eSaved)
      save_state = SaveState::eUnsaved;
  }

  void Scene::redo_mod() {
    met_trace();
    
    guard(mod_i < (int(mods.size()) - 1));
    
    mod_i += 1;
    mods[mod_i].redo(*this);

    if (save_state == SaveState::eSaved)
      save_state = SaveState::eUnsaved;
  }

  void Scene::undo_mod() {
    met_trace();
    
    guard(mod_i >= 0);

    mods[mod_i].undo(*this);
    mod_i -= 1;

    if (save_state == SaveState::eSaved)
      save_state = SaveState::eUnsaved;
  }

  void Scene::clear_mods() {
    met_trace();

    mods  = { };
    mod_i = -1;
  }

  met::ColrSystem Scene::csys(const Uplifting &uplifting) const {
    met_trace();
    return csys(uplifting.observer_i, uplifting.illuminant_i);
  }

  std::string Scene::csys_name(const Uplifting &uplifting) const {
    met_trace();
    return csys_name(uplifting.observer_i, uplifting.illuminant_i);
  }

  met::ColrSystem Scene::csys(uint cmfs_i, uint illm_i) const {
    met_trace();
    return ColrSystem { .cmfs = *resources.observers[cmfs_i], .illuminant = *resources.illuminants[illm_i] };
  }

  met::Spec Scene::emitter_spd(uint i) const {
    met_trace();
    return emitter_spd(components.emitters[i].value);
  }

  met::Spec Scene::emitter_spd(Emitter e) const {
    met_trace();
    return (resources.illuminants[e.illuminant_i].value() * e.illuminant_scale).eval();
  }

  CMFS Scene::primary_observer() const {
    met_trace();
    return view_observer(components.settings->view_i);
  }
  
  met::CMFS Scene::view_observer(uint i) const {
    met_trace();
    return view_observer(components.views[i].value);
  }

  met::CMFS Scene::view_observer(View i) const {
    met_trace();
    return resources.observers[i.observer_i].value();
  }

  std::string Scene::csys_name(uint cmfs_i, uint illm_i) const {
    met_trace();
    return fmt::format("{}, {}", 
                       resources.observers[cmfs_i].name, 
                       resources.illuminants[illm_i].name);
  }

  const Uplifting::Vertex &Scene::uplifting_vertex(ConstraintRecord cs) const {
    return components.upliftings[cs.uplifting_i]->verts[cs.vertex_i];
  }

  Uplifting::Vertex &Scene::uplifting_vertex(ConstraintRecord cs) {
    return components.upliftings[cs.uplifting_i]->verts[cs.vertex_i];
  }

  void Scene::update() {
    met_trace_full();

    // Force check of scene indices to ensure linked components/resources still exist,
    // or reset to indices that we know exist
    for (auto [i, comp] : enumerate_view(components.objects)) {
      auto &obj = comp.value;
      if (obj.mesh_i >= resources.meshes.size())
        obj.mesh_i = 0u;
      if (obj.uplifting_i >= components.upliftings.size())
        obj.uplifting_i = 0u;

      obj.diffuse | visit_single([&](uint i) {
        if (i >= resources.images.size())
          obj.diffuse = Colr(0.5f);
      });
      obj.roughness | visit_single([&](uint i) {
        if (i >= resources.images.size())
          obj.roughness = 0.1f;
      });
      obj.metallic | visit_single([&](uint i) {
        if (i >= resources.images.size())
          obj.metallic = 0.0f;
      });
    }
    for (auto [i, comp] : enumerate_view(components.emitters)) {
      auto &emt = comp.value;
      if (emt.illuminant_i >= resources.illuminants.size())
        emt.illuminant_i = 0u;
    }
    for (auto [i, comp] : enumerate_view(components.upliftings)) {
      auto &upl = comp.value;
      if (upl.observer_i >= resources.observers.size())
        upl.observer_i = 0u;
      if (upl.illuminant_i >= resources.illuminants.size())
        upl.illuminant_i = 0u;
    }
    {
      auto &settings = components.settings.value;
      if (settings.view_i >= components.views.size())
        settings.view_i = 0u;
    }

    // This one first; a lot of things query it, e.g. images for texture size
    components.settings.state.update(components.settings.value);

    // Force update check of stale gl-side components and state tracking
    resources.meshes.update(*this);
    resources.images.update(*this);
    resources.illuminants.update(*this);
    resources.observers.update(*this);
    resources.bases.update(*this);
    components.emitters.update(*this);
    components.objects.update(*this);
    components.upliftings.update(*this);
    components.views.update(*this);
  }

  void Scene::wait_for_update() const {
    met_trace_full();

    // Forcibly wait on all initialized sync objects; most components/resources
    // never initialize these, and the operation will do nothing
    resources.meshes.gl.wait_for_update();
    resources.images.gl.wait_for_update();
    resources.illuminants.gl.wait_for_update();
    resources.observers.gl.wait_for_update();
    resources.bases.gl.wait_for_update();
    components.emitters.gl.wait_for_update();
    components.objects.gl.wait_for_update();
    components.upliftings.gl.wait_for_update();
    components.views.gl.wait_for_update();
  }
} // namespace met