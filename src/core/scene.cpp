#include <metameric/core/io.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/serialization.hpp>
#include <metameric/core/tree.hpp>
#include <metameric/core/utility.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <nlohmann/json.hpp>
#include <zstr.hpp>
#include <algorithm>
#include <execution>
#include <format>
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
    template <typename Ty>
    void to_json(json &js, const detail::Component<Ty> &component) {
      met_trace();
      js = {{ "name",  component.name  },
            { "value", component.value }};
    }

    template <typename Ty>
    void from_json(const json &js, detail::Component<Ty> &component) {
      met_trace();
      js.at("name").get_to(component.name);
      js.at("value").get_to(component.value);
    }

    eig::Vector3f gen_barycentric_coords(eig::Vector3f p, 
                                         eig::Vector3f a, 
                                         eig::Vector3f b, 
                                         eig::Vector3f c) {
      met_trace();

      eig::Vector3f ab = b - a, ac = c - a;

      float a_tri = std::abs(.5f * ac.cross(ab).norm());
      float a_ab  = std::abs(.5f * (p - a).cross(ab).norm());
      float a_ac  = std::abs(.5f * ac.cross(p - a).norm());
      float a_bc  = std::abs(.5f * (c - p).cross(b - p).norm());

      return (eig::Vector3f(a_bc, a_ac, a_ab) / a_tri).eval();
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
      case 0: vert.constraint = js.at("variant").get<DirectColorConstraint>(); break;
      case 1: vert.constraint = js.at("variant").get<MeasurementConstraint>(); break;
      case 2: vert.constraint = js.at("variant").get<DirectSurfaceConstraint>(); break;
      case 3: vert.constraint = js.at("variant").get<IndirectSurfaceConstraint>(); break;
      default: debug::check_expr(false, "Error parsing json constraint data");
    }
  }

  void to_json(json &js, const Uplifting &uplifting) {
    met_trace();
    js = {{ "csys_i",  uplifting.csys_i  },
          { "basis_i", uplifting.basis_i },
          { "verts",   uplifting.verts   }};
  }

  void from_json(const json &js, Uplifting &uplifting) {
    met_trace();
    js.at("csys_i").get_to(uplifting.csys_i);
    js.at("basis_i").get_to(uplifting.basis_i);
    js.at("verts").get_to(uplifting.verts);
  }

  void to_json(json &js, const Settings &settings) {
    met_trace();
    js = {{ "texture_size", settings.texture_size }};
  }

  void from_json(const json &js, Settings &settings) {
    met_trace();
    js.at("texture_size").get_to(settings.texture_size);
  }

  void to_json(json &js, const Object &object) {
    met_trace();
    js = {{ "is_active",   object.is_active   },
          { "transform",   object.transform   },
          { "mesh_i",      object.mesh_i      },
          { "uplifting_i", object.uplifting_i }};
      
    js["diffuse"]   = {{ "index", object.diffuse.index() },   { "variant", object.diffuse }};
    /* js["roughness"] = {{ "index", object.roughness.index() }, { "variant", object.roughness }};
    js["metallic"]  = {{ "index", object.metallic.index() },  { "variant", object.metallic }};
    js["opacity"]   = {{ "index", object.opacity.index() },   { "variant", object.opacity }};
    js["normals"]   = {{ "index", object.normals.index() },   { "variant", object.normals }}; */
  }

  void from_json(const json &js, Object &object) {
    met_trace();
    js.at("is_active").get_to(object.is_active);
    js.at("transform").get_to(object.transform);
    js.at("mesh_i").get_to(object.mesh_i);
    js.at("uplifting_i").get_to(object.uplifting_i);
    switch (js.at("diffuse").at("index").get<size_t>()) {
      case 0: object.diffuse = js.at("diffuse").at("variant").get<Colr>(); break;
      case 1: object.diffuse = js.at("diffuse").at("variant").get<uint>(); break;
      default: debug::check_expr(false, "Error parsing json material data");
    }
    /* switch (js.at("roughness").at("index").get<size_t>()) {
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
    } */
  }

  void to_json(json &js, const Emitter &emitter) {
    met_trace();
    js = {{ "type",             emitter.type             },
          { "transform",        emitter.transform        },
          { "is_active",        emitter.is_active        },
          { "illuminant_i",     emitter.illuminant_i     },
          { "illuminant_scale", emitter.illuminant_scale }};
  }

  void from_json(const json &js, Emitter &emitter) {
    met_trace();
    js.at("type").get_to(emitter.type);
    js.at("is_active").get_to(emitter.is_active);
    js.at("transform").get_to(emitter.transform);
    js.at("illuminant_i").get_to(emitter.illuminant_i);
    js.at("illuminant_scale").get_to(emitter.illuminant_scale);
  }

  void to_json(json &js, const ColorSystem &csys) {
    met_trace();
    js = {{ "observer_i",   csys.observer_i   },
          { "illuminant_i", csys.illuminant_i },
          { "n_scatters",   csys.n_scatters   }};
  }

  void from_json(const json &js, ColorSystem &csys) {
    met_trace();
    js.at("observer_i").get_to(csys.observer_i);
    js.at("illuminant_i").get_to(csys.illuminant_i);
    js.at("n_scatters").get_to(csys.n_scatters);
  }

  void to_json(json &js, const Scene &scene) {
    met_trace();
    js = {{ "settings",      scene.components.settings            },
          { "observer_i",    scene.components.observer_i          },
          { "objects",       scene.components.objects.data()      },
          { "emitters",      scene.components.emitters.data()     },
          { "upliftings",    scene.components.upliftings.data()   },
          { "colr_systems",  scene.components.colr_systems.data() }};
  }

  void from_json(const json &js, Scene &scene) {
    met_trace();
    js.at("settings").get_to(scene.components.settings);
    js.at("observer_i").get_to(scene.components.observer_i);
    js.at("objects").get_to(scene.components.objects.data());
    js.at("emitters").get_to(scene.components.emitters.data());
    js.at("upliftings").get_to(scene.components.upliftings.data());
    js.at("colr_systems").get_to(scene.components.colr_systems.data());
  }
  
  void Scene::create() {
    met_trace();
 
    // Clear out scene first
    *this = { };

    // Pre-supply some data for an initial scene
    auto loaded_tree = io::load_json("resources/misc/tree.json").get<BasisTreeNode>();
    components.settings   = { .name  = "Settings", 
                              .value = { .texture_size = Settings::TextureSize::eHigh }};
    components.observer_i = { .name  = "Default observer", 
                              .value = 0 };
    resources.bases.push("Default basis",  loaded_tree.basis,           false);
    resources.illuminants.push("D65",      models::emitter_cie_d65,     false);
    resources.illuminants.push("E",        models::emitter_cie_e,       false);
    resources.illuminants.push("FL2",      models::emitter_cie_fl2,     false);
    resources.illuminants.push("FL11",     models::emitter_cie_fl11,    false);
    resources.illuminants.push("LED-RGB1", models::emitter_cie_ledrgb1, false);
    resources.illuminants.push("LED-B1",   models::emitter_cie_ledb1,   false);
    resources.observers.push("CIE XYZ",    models::cmfs_cie_xyz,        false);
    resources.meshes.push("Rectangle",     models::unit_rect,           false);

    // Default color system
    ColorSystem csys { .observer_i = 0, .illuminant_i = 0, .n_scatters = 0 };
    components.colr_systems.push(get_csys_name(csys), csys);
    
    // Default uplifting
    components.upliftings.emplace("Default uplifting", { .csys_i = 0, .basis_i = 0 });

    // Default emitter
    components.emitters.push("Default D65 emitter", {
      .type             = Emitter::Type::eRect,
      .transform        = { .position = { -0.5f, 0.98f, -0.5f },
                            .rotation = { 90.f * std::numbers::pi_v<float> / 180.f, 0.f, 0.f },
                            .scaling  = 0.2f },
      .illuminant_i     = 0,
      .illuminant_scale = 1.f
    });
    
    // Set state to fresh create
    save_path  = "";
    save_state = SaveState::eNew;
    clear_mods();
  }

  void Scene::unload() {
    met_trace();

    // Clear out scene first
    *this = { };

    // Set state to unloaded
    save_path  = "";
    save_state = SaveState::eUnloaded;
    clear_mods();
  }

  void Scene::save(const fs::path &path) {
    met_trace();

    // Get paths to .json and .data files with matching extensions
    fs::path json_path = io::path_with_ext(path, ".json");
    fs::path data_path = io::path_with_ext(path, ".data");
      
    // Attempt opening zlib compressed stream, and serialize scene resources
    auto str = zstr::ofstream(data_path.string(), scene_o_flags, Z_BEST_SPEED);
    debug::check_expr(str.good());
    to_stream(str);

    // Attempt serialize and save of scene object to .json file
    json js = *this;
    io::save_json(json_path, js);

    // Set state to fresh save
    save_path  = io::path_with_ext(path, ".json");
    save_state = SaveState::eSaved;
  }

  void Scene::load(const fs::path &path) {
    met_trace();
    
    // Clear out scene first
    *this = { };

    // Get paths to .json and .data files with matching extensions
    fs::path json_path = io::path_with_ext(path, ".json");
    fs::path data_path = io::path_with_ext(path, ".data");

    // Attempt load and deserialize of .json file to initial scene object
    json js = io::load_json(json_path);
    js.get_to(*this);

    // Next, attempt opening zlib compressed stream, and deserialize to scene object
    auto str = zstr::ifstream(data_path.string(), scene_i_flags);
    debug::check_expr(str.good());
    fr_stream(str);
      
    // Set state to fresh load
    save_path  = io::path_with_ext(path, ".json");
    save_state = SaveState::eSaved;
    clear_mods();
  }

  void Scene::import_scene(const fs::path &path) {
    met_trace();
    Scene other;
    other.load(path);
    import_scene(std::move(other));
  }

  void Scene::import_scene(Scene &&other) {
    // Import scene objects/emitters/materials/etc, taking care to increment indexes while bookkeeping correctly
    std::transform(range_iter(other.components.upliftings), 
                   std::back_inserter(components.upliftings.data()), [&](auto component) {
      component.value.csys_i += components.colr_systems.size();
      // TODO update bookkeeping
      /* for (auto &vert : component.value.verts) {
        for (auto &j : vert.csys_j)
          j += components.colr_systems.size();
        if (vert.type == UpliftingConstraint::Type::eColorOnMesh) {
          vert.object_i += components.objects.size();
        }
      } */
      return component;
    });
    std::transform(range_iter(other.components.objects), 
                   std::back_inserter(components.objects.data()), [&](auto component) {
      component.value.mesh_i += resources.meshes.size();
      if (component.value.diffuse.index() == 1)
        component.value.diffuse = static_cast<uint>(std::get<1>(component.value.diffuse) + resources.images.size());
      /* if (component.value.roughness.index() == 1)
        component.value.roughness = static_cast<uint>(std::get<1>(component.value.roughness) + resources.images.size());
      if (component.value.metallic.index() == 1)
        component.value.metallic = static_cast<uint>(std::get<1>(component.value.metallic) + resources.images.size());
      if (component.value.opacity.index() == 1)
        component.value.opacity = static_cast<uint>(std::get<1>(component.value.opacity) + resources.images.size());
      if (component.value.normals.index() == 1)
        component.value.normals = static_cast<uint>(std::get<1>(component.value.normals) + resources.images.size()); */
      if (!other.components.upliftings.empty())
        component.value.uplifting_i += components.upliftings.size();
      return component;
    });
    std::transform(range_iter(other.components.emitters), 
                   std::back_inserter(components.emitters.data()), [&](auto component) {
      if (!other.resources.illuminants.empty())
        component.value.illuminant_i += resources.illuminants.size();
      return component;
    });
    std::transform(range_iter(other.components.colr_systems), 
                   std::back_inserter(components.colr_systems.data()), [&](auto component) {
      if (!other.resources.observers.empty())
        component.value.observer_i   += resources.observers.size();
      if (!other.resources.illuminants.empty())
        component.value.illuminant_i += resources.illuminants.size();
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

  void Scene::export_uplifting(const fs::path &path, uint uplifting_i) const {
    met_trace();

    debug::check_expr(false, "Not implemented!");
  }
  
  void Scene::import_wavefront_obj(const fs::path &path) {
    met_trace();

    debug::check_expr(fs::exists(path), 
      std::format("File at \"{}\" does not appear to exist", path.string()));

    // Attempt to import the .OBJ file using ASSIMP
    Assimp::Importer imp;
    const auto *file = imp.ReadFile(path.string(), 
      aiProcess_Triangulate             | 
      // aiProcess_GenSmoothNormals        |
      aiProcess_FlipUVs                 |
      aiProcess_RemoveRedundantMaterials);

    debug::check_expr(file, 
      std::format("File at \"{}\" could not be read. ASSIMP says: \"{}\"\n", 
      path.string(), std::string(imp.GetErrorString())));

    std::span file_meshes    = { file->mMeshes, file->mNumMeshes };
    std::span file_textures  = { file->mTextures, file->mNumTextures };
    std::span file_materials = { file->mMaterials, file->mNumMaterials };

    // Temporary scene to which we add all imported objects
    Scene scene;

    // Loading caches; prevent unnecessary image and material loads
    std::unordered_map<uint, uint>     material_uuid;
    std::unordered_map<fs::path, uint> image_uuid;

    // First, build a list of used mesh objects by traversing assimp tree;
    // not all materials in an OBJ file are used, and we don't want to clutter
    // the scene with unused imports
    {
      struct QueueObject {
        eig::Matrix4f trf;
        aiNode       *node;
      } root = { eig::Matrix4f::Identity(), file->mRootNode };

      std::deque<QueueObject> queue = { root };
      while (!queue.empty()) {
        // Pop current node from work queue
        auto [parent_trf, node] = queue.front();
        queue.pop_front();

        // Assemble recursive transformation to pass to children
        eig::Matrix4f trf;
        std::memcpy(trf.data(), (const void *) &(node->mTransformation), sizeof(aiMatrix4x4));
        trf = parent_trf * trf;
        eig::Affine3f aff(trf);

        // If current node has meshes attached, register object(s)
        for (uint i : std::span { node->mMeshes, node->mNumMeshes }) {
          // Register mesh material if not yet registered
          uint material_i = file->mMeshes[i]->mMaterialIndex;
          if (!material_uuid.contains(material_i))
            material_uuid[material_i] = material_uuid.size();
          material_i = material_uuid[material_i];

          scene.components.objects.emplace(node->mName.C_Str(), {
            .transform   = Transform::from_affine(aff),
            .mesh_i      = i,
            .uplifting_i = 0,
          });
        }
        
        // Push child nodes on queue for processing
        for (auto child : std::span { node->mChildren, node->mNumChildren })
          queue.push_back({ trf, child });
      }
    }

    // Process included meshes in order
    for (const auto *mesh : file_meshes) {
      Mesh m;

      if (mesh->HasPositions()) {
        std::span verts = { mesh->mVertices, mesh->mNumVertices };
        m.verts.resize(verts.size());
        std::transform(std::execution::par_unseq, range_iter(verts), m.verts.begin(),
          [](const auto &v) { return Mesh::vert_type { v.x, v.y, v.z }; });
      }
    
      if (mesh->HasNormals()) {
        std::span norms = { mesh->mNormals, mesh->mNumVertices };
        m.norms.resize(norms.size());
        std::transform(std::execution::par_unseq, range_iter(norms), m.norms.begin(),
          [](const auto &v) { return Mesh::norm_type { v.x, v.y, v.z }; });
      }

      uint tx_count = 0;
      for (uint i = 0; i < 16; ++i)
        tx_count += mesh->HasTextureCoords(i) ? 1 : 0;
      fmt::print("num texture coords; {}\n", tx_count);
      
      // Assume first set of coords is used only
      constexpr size_t default_texture_coord = 0;
      if (mesh->HasTextureCoords(default_texture_coord)) {
        std::span txuvs = { mesh->mTextureCoords[default_texture_coord], mesh->mNumVertices };
        m.txuvs.resize(txuvs.size());
        std::transform(std::execution::par_unseq, range_iter(txuvs), m.txuvs.begin(),
          [](const auto &v) { return Mesh::txuv_type { v.x, v.y }; });
      }

      if (mesh->HasFaces()) {
        std::span elems = { mesh->mFaces, mesh->mNumFaces };
        m.elems.resize(elems.size());
        std::transform(std::execution::par_unseq, range_iter(elems), m.elems.begin(),
          [](const aiFace &v) { return Mesh::elem_type { v.mIndices[0], v.mIndices[1], v.mIndices[2] }; });
      }

      // Ensure mesh data is properly corrected and redundant vertices are stripped
      remap_mesh(m);
      compact_mesh(m);

      scene.resources.meshes.emplace(mesh->mName.C_Str(), std::move(m));
    }

    // Process object material data in order
    for (auto &component : scene.components.objects) {
      auto &object = component.value;

      // Get referred material index of mesh
      const auto *material = file_materials[file_meshes[object.mesh_i]->mMaterialIndex];

      // First define default material properties, assuming no succesful loads
      aiColor3D baseColorValue(1);
      ai_real metallicValue(1), roughnessValue(1), opacityValue(1);

      // Attempt to fetch a diffuse color property from the material
      if (aiReturn_SUCCESS != material->Get(AI_MATKEY_BASE_COLOR, baseColorValue))
        material->Get(AI_MATKEY_COLOR_DIFFUSE, baseColorValue);

      // Attempt to fetch miscellaneous properties
      /* material->Get(AI_MATKEY_METALLIC_FACTOR, metallicValue);
      material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughnessValue);
      material->Get(AI_MATKEY_OPACITY, opacityValue); */

      // Assuming texture file paths are available, next attempt to gather these
      aiString baseColorTexture, metallicTexture, roughnessTexture, opacityTexture, normalTexture;

      // Search for a corresponding texture path for diffuse
      for (auto tag : { aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE }) {
        material->GetTexture(tag, 0, &baseColorTexture);
        guard_break(baseColorTexture.length == 0);
      }
        
      // Search for a corresponding texture path for normal maps
      /* for (auto tag : { aiTextureType_NORMALS, aiTextureType_HEIGHT, aiTextureType_NORMAL_CAMERA }) {
        material->GetTexture(tag, 0, &normalTexture);
        guard_break(normalTexture.length == 0);
      } */

      // Search for texture paths for miscellaneous values
      /* material->GetTexture(aiTextureType_METALNESS,         0, &metallicTexture);
      material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &roughnessTexture);
      material->GetTexture(aiTextureType_OPACITY,           0, &opacityTexture); */

      // Texture image load helper
      auto image_load = [&](auto &target, std::string_view image_str, auto image_value) {
        if (auto image_path = path.parent_path() / image_str; !image_str.empty() && fs::exists(image_path)) {
          if (auto it = image_uuid.find(image_path); it != image_uuid.end()) {
            target = it->second;
          } else {
            scene.resources.images.emplace(image_path.filename().string(), {{ .path = image_path }});
            target = static_cast<uint>(scene.resources.images.size() - 1);
          }
        } else {
          target = image_value;
        }
      };

      // Attempt to load all referred texture images into their variant, or instead use the provided value
      image_load(object.diffuse,   baseColorTexture.C_Str(), Colr { baseColorValue.r, baseColorValue.g, baseColorValue.b });
      /* image_load(object.metallic,  metallicTexture.C_Str(), metallicValue);
      image_load(object.roughness, roughnessTexture.C_Str(), roughnessValue);
      image_load(object.opacity,   opacityTexture.C_Str(), opacityValue);
      image_load(object.normals,   normalTexture.C_Str(), Colr { 0, 0, 1 }); */
    }

    import_scene(std::move(scene));
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

  met::ColrSystem Scene::get_csys(uint i) const {
    met_trace();
    return get_csys(components.colr_systems[i].value);
  }

  met::ColrSystem Scene::get_csys(ColorSystem c) const {
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
    return (resources.illuminants[e.illuminant_i].value() * e.illuminant_scale).eval();
  }

  std::string Scene::get_csys_name(uint i) const {
    met_trace();
    return get_csys_name(components.colr_systems[i].value);
  }

  std::string Scene::get_csys_name(ColorSystem c) const {
    met_trace();
    return std::format("{}, {}", 
                       resources.observers[c.observer_i].name, 
                       resources.illuminants[c.illuminant_i].name);
  }

  
  std::pair<Colr, Spec> Scene::get_uplifting_constraint(uint i, uint vert_i) const {
    met_trace();
    return get_uplifting_constraint(components.upliftings[i].value,
                                    components.upliftings[i].value.verts[vert_i]);
  }

  std::pair<Colr, Spec> Scene::get_uplifting_constraint(const Uplifting &u, const Uplifting::Vertex &v) const {
    met_trace();

    // Return zero constraint for inactive parts
    if (!v.is_active)
      return { Colr(0.f), Spec(0.f) };

    // Color system spectra within which the 'uplifted' texture is defined
    CMFS csys_i = get_csys(u.csys_i).finalize_direct();

    // Output values
    Colr c;
    Spec s;

    // Identify the type of constraint
    if (const auto *constraint = std::get_if<DirectColorConstraint>(&v.constraint)) {
      // The specified color becomes our vertex color
      c = constraint->colr_i;

      // Gather all relevant color system spectra referred by the constraint
      std::vector<CMFS> systems = { csys_i };
      rng::transform(constraint->csys_j, std::back_inserter(systems), 
        [&](uint j) { return get_csys(j).finalize_direct(); });
      
      // Obtain corresponding color constraints for each color system
      std::vector<Colr> signals = { c };
      rng::copy(constraint->csys_j, std::back_inserter(signals));

      // Generate a metamer satisfying the system+signal constraint set
      s = generate_spectrum({
        .basis   = resources.bases[u.basis_i].value(),
        .systems = systems,
        .signals = signals
      });
    } else if (const auto *constraint = std::get_if<MeasurementConstraint>(&v.constraint)) {
      // The specified spectrum becomes our metamer
      s = constraint->measurement;

      // The metamer's color under the uplifting's color system becomes our vertex color
      c = (csys_i.transpose() * s.matrix()).eval();
    } else if (const auto *constraint = std::get_if<DirectSurfaceConstraint>(&v.constraint)) {
      // Return zero constraint for invalid surfaces
      if (!constraint->is_valid())
        return { Colr(0.f), Spec(0.f) };

      // Color is obtained from surface information
      c = constraint->surface.diffuse;

      // Gather all relevant color system spectra referred by the constraint
      std::vector<CMFS> systems = { csys_i };
      rng::transform(constraint->csys_j, std::back_inserter(systems), 
        [&](uint j) { return get_csys(j).finalize_direct(); });

      // Obtain corresponding color constraints for each color system
      std::vector<Colr> signals = { c };
      rng::copy(constraint->csys_j, std::back_inserter(signals));

      // Generate a metamer satisfying the system+signal constraint set
      s = generate_spectrum({
        .basis   = resources.bases[u.basis_i].value(),
        .systems = systems,
        .signals = signals
      });
    } else if (const auto *constraint = std::get_if<IndirectSurfaceConstraint>(&v.constraint)) {
      debug::check_expr(false, "Not implemented!");
      // TODO ...
    }

    return { c, s };
  }

  SurfaceInfo Scene::get_surface_info(const RayRecord &ray) const {
    met_trace();

    // Return object; return early if an invalid object was given
    SurfaceInfo si = { .record = ray.record };
    guard(si.is_valid(), si);
    
    // Get relevant resources; mostly gl-side resources
    const auto &object    = components.objects[si.record.object_i()].value;
    const auto &object_gl = components.objects.gl.objects()[ray.record.object_i()];
    const auto &prim      = resources.meshes.gl.bvh_prims_cpu[ray.record.primitive_i()].unpack();
  
    // Get transforms used for gl-side world-model space
    auto trf = object_gl.trf_mesh;
    auto inv = object_gl.trf_mesh_inv;

    // Generate barycentric coordinates
    eig::Vector3f p    = ray.get_position();
    eig::Vector3f pinv = (inv * eig::Vector4f(p.x(), p.y(), p.z(), 1.f)).head<3>();
    auto bary = detail::gen_barycentric_coords(pinv, prim.v0.p, prim.v1.p, prim.v2.p);

    // Recover surface geometric data
    si.p  = bary.x() * prim.v0.p  + bary.y() * prim.v1.p  + bary.z() * prim.v2.p;
    si.n  = bary.x() * prim.v0.n  + bary.y() * prim.v1.n  + bary.z() * prim.v2.n;
    si.tx = bary.x() * prim.v0.tx + bary.y() * prim.v1.tx + bary.z() * prim.v2.tx;
    si.p  = (trf * eig::Vector4f(si.p.x(), si.p.y(), si.p.z(), 1.f)).head<3>();
    si.n  = (trf * eig::Vector4f(si.n.x(), si.n.y(), si.n.z(), 0.f)).head<3>();
    si.n.normalize();

    // Recover surface diffuse data based on underlying object material
    si.diffuse = std::visit(overloaded {
      [&](const uint &i) {
        const auto &txtr = resources.images[i].value();
        return txtr.sample(si.tx, Image::ColorFormat::eLRGB).head<3>().eval();
      },
      [](const Colr &c) { return c; }
    }, object.diffuse);

    return si;
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
} // namespace met