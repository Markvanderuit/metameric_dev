#include <metameric/core/scene_handler.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/tree.hpp>
#include <metameric/core/utility.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <execution>
#include <format>
#include <unordered_map>
#include <unordered_set>
#include <queue>

namespace met {
  void SceneHandler::create() {
    met_trace();

    // Clear out all scene handler state first
    scene      = { };
    save_path  = "";
    save_state = SaveState::eNew;
    clear_mods();

    // Pre-supply some data for an initial scene
    auto loaded_tree = io::load_json("resources/misc/tree.json").get<BasisTreeNode>();
    scene.bases.emplace("Default basis", {
      .mean      = loaded_tree.basis_mean,
      .functions = loaded_tree.basis
    });
    scene.observer_i = { .name = "Default observer", .value = 0 };
    scene.illuminants.push("D65",      models::emitter_cie_d65);
    scene.illuminants.push("E",        models::emitter_cie_e);
    scene.illuminants.push("FL2",      models::emitter_cie_fl2);
    scene.illuminants.push("FL11",     models::emitter_cie_fl11);
    scene.illuminants.push("LED-RGB1", models::emitter_cie_ledrgb1);
    scene.observers.push("CIE XYZ",    models::cmfs_cie_xyz);
    scene.upliftings.emplace("Default uplifting", {
      .type    = Uplifting::Type::eDelaunay,
      .basis_i = 0
    });
    Scene::ColrSystem csys { .observer_i = 0, .illuminant_i = 0, .n_scatters = 0 };
    scene.colr_systems.push(scene.get_csys_name(csys), csys);
  }

  void SceneHandler::save(const fs::path &path) {
    met_trace();

    io::save_scene(path, scene);

    save_path  = io::path_with_ext(path, ".json");
    save_state = SaveState::eSaved;
  }

  void SceneHandler::load(const fs::path &path) {
    met_trace();

    scene      = io::load_scene(path);
    save_path  = io::path_with_ext(path, ".json");
    save_state = SaveState::eSaved;

    clear_mods();
  }

  void SceneHandler::unload() {
    met_trace();

    scene      = { };
    save_path  = "";
    save_state = SaveState::eUnloaded;

    clear_mods();
  }

  void SceneHandler::touch(SceneHandler::SceneMod &&mod) {
    met_trace();

    // Apply change
    mod.redo(scene);
    fmt::print("Touch!\n");

    // Ensure mod list doesn't exceed fixed length
    // and set the current mod to its end
    int n_mods = std::clamp(mod_i + 1, 0, 128);
    mod_i = n_mods;
    mods.resize(mod_i);
    mods.push_back(mod);   
    
    if (save_state == SaveState::eSaved)
      save_state = SaveState::eUnsaved;
  }

  void SceneHandler::redo_mod() {
    met_trace();
    
    guard(mod_i < (int(mods.size()) - 1));
    
    mod_i += 1;
    mods[mod_i].redo(scene);

    if (save_state == SaveState::eSaved)
      save_state = SaveState::eUnsaved;
  }

  void SceneHandler::undo_mod() {
    met_trace();
    
    guard(mod_i >= 0);

    mods[mod_i].undo(scene);
    mod_i -= 1;

    if (save_state == SaveState::eSaved)
      save_state = SaveState::eUnsaved;
  }

  void SceneHandler::clear_mods() {
    met_trace();

    mods  = { };
    mod_i = -1;
  }

  void SceneHandler::export_uplifting(const fs::path &path, uint uplifting_i) const {
    met_trace();

    debug::check_expr(false, "Not implemented!");
  }

  void SceneHandler::import_wavefront_obj(const fs::path &path) {
    met_trace();

    debug::check_expr(fs::exists(path), 
      std::format("File at \"{}\" does not appear to exist", path.string()));

    // Attempt to import the .OBJ file using ASSIMP
    Assimp::Importer imp;
    const auto *file = imp.ReadFile(path.string(), 
      aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals);

    debug::check_expr(file, 
      std::format("File at \"{}\" could not be read. ASSIMP says: \"{}\"\n", 
      path.string(), std::string(imp.GetErrorString())));

    std::span file_meshes    = { file->mMeshes, file->mNumMeshes };
    std::span file_textures  = { file->mTextures, file->mNumTextures };
    std::span file_materials = { file->mMaterials, file->mNumMaterials };

    // Temporary scene to which we add all imported objects
    Scene scene;

    // Loading caches; prevent unnecessary texture and material loads
    std::unordered_map<uint, uint>     material_uuid;
    std::unordered_map<fs::path, uint> texture_uuid;

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

        // If current node has meshes attached, register object(s)
        for (uint i : std::span { node->mMeshes, node->mNumMeshes }) {
          // Register mesh material if not yet registered
          uint material_i = file->mMeshes[i]->mMaterialIndex;
          if (!material_uuid.contains(material_i))
            material_uuid[material_i] = material_uuid.size();
          material_i = material_uuid[material_i];

          scene.objects.emplace(node->mName.C_Str(), {
            .mesh_i      = i,
            .material_i  = material_i,
            .uplifting_i = 0 ,
            .trf         = eig::Affine3f(trf)
          });
        }
        
        // Push child nodes on queue for processing
        for (auto child : std::span { node->mChildren, node->mNumChildren })
          queue.push_back({ trf, child });
      }
    }

    // Process included meshes in order
    for (const auto *mesh : file_meshes) {
      AlMeshData m;

      if (mesh->HasPositions()) {
        std::span verts = { mesh->mVertices, mesh->mNumVertices };
        m.verts.resize(verts.size());
        std::transform(std::execution::par_unseq, range_iter(verts), m.verts.begin(),
          [](const auto &v) { return AlMeshData::VertTy { v.x, v.y, v.z }; });
      }
    
      if (mesh->HasNormals()) {
        std::span norms = { mesh->mNormals, mesh->mNumVertices };
        m.norms.resize(norms.size());
        std::transform(std::execution::par_unseq, range_iter(norms), m.norms.begin(),
          [](const auto &v) { return AlMeshData::NormTy { v.x, v.y, v.z }; });
      }

      // Assume first set of coords is used only
      constexpr size_t default_texture_coord = 0;
      if (mesh->HasTextureCoords(default_texture_coord)) {
        std::span uvs = { mesh->mTextureCoords[default_texture_coord], mesh->mNumVertices };
        m.uvs.resize(uvs.size());
        std::transform(std::execution::par_unseq, range_iter(uvs), m.uvs.begin(),
          [](const auto &v) { return AlMeshData::UVTy { v.x, v.y }; });
      }

      if (mesh->HasFaces()) {
        std::span elems = { mesh->mFaces, mesh->mNumFaces };
        m.elems.resize(elems.size());
        std::transform(std::execution::par_unseq, range_iter(elems), m.elems.begin(),
          [](const aiFace &v) { return AlMeshData::ElemTy { v.mIndices[0], v.mIndices[1], v.mIndices[2] }; });
      }

      scene.meshes.emplace(mesh->mName.C_Str(), std::move(m));
    }

    // Process included materials in order
    scene.materials.resize(material_uuid.size());
    for (auto [material_i_old, material_i_new] : material_uuid) {
      const auto *material = file_materials[material_i_old];
      Scene::Material m;

      // First define default material properties, assuming no succesful loads
      aiColor3D baseColorValue(1);
      ai_real metallicValue(1), roughnessValue(1), opacityValue(1);

      // Attempt to fetch a diffuse color property from the material
      if (aiReturn_SUCCESS != material->Get(AI_MATKEY_BASE_COLOR, baseColorValue))
        material->Get(AI_MATKEY_COLOR_DIFFUSE, baseColorValue);

      // Attempt to fetch miscellaneous properties
      material->Get(AI_MATKEY_METALLIC_FACTOR, metallicValue);
      material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughnessValue);
      material->Get(AI_MATKEY_OPACITY, opacityValue);

      // Assuming texture file paths are available, next attempt to gather these
      aiString baseColorTexture, metallicTexture, roughnessTexture, opacityTexture, normalTexture;

      // Search for a corresponding texture path for diffuse
      for (auto tag : { aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE }) {
        material->GetTexture(tag, 0, &baseColorTexture);
        guard_break(baseColorTexture.length == 0);
      }
        
      // Search for a corresponding texture path for normal maps
      for (auto tag : { aiTextureType_NORMALS, aiTextureType_HEIGHT, aiTextureType_NORMAL_CAMERA }) {
        material->GetTexture(tag, 0, &normalTexture);
        guard_break(normalTexture.length == 0);
      }

      // Search for texture paths for miscellaneous values
      material->GetTexture(aiTextureType_METALNESS,         0, &metallicTexture);
      material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &roughnessTexture);
      material->GetTexture(aiTextureType_OPACITY,           0, &opacityTexture);

      // Texture load helpers
      auto texture_load_3f = [&](auto &variant, std::string_view texture_str, auto texture_value) {
        if (auto texture_path = path.parent_path() / texture_str; !texture_str.empty() && fs::exists(texture_path)) {
          if (auto it = texture_uuid.find(texture_path); it != texture_uuid.end()) {
            variant = it->second;
          } else {
            auto texture_data = io::load_texture2d<Colr>(texture_path, true);
            scene.textures_3f.emplace(baseColorTexture.C_Str(), std::move(texture_data));
            variant = static_cast<uint>(scene.textures_3f.size() - 1);
          }
        } else {
          variant = texture_value;
        }
      };
      auto texture_load_1f = [&](auto &variant, std::string_view texture_str, auto texture_value) {
        if (auto texture_path = path.parent_path() / texture_str; !texture_str.empty() && fs::exists(texture_path)) {
          if (auto it = texture_uuid.find(texture_path); it != texture_uuid.end()) {
            variant = it->second;
          } else {
            auto texture_data = io::load_texture2d<eig::Array1f>(texture_path, true);
            scene.textures_1f.emplace(baseColorTexture.C_Str(), std::move(texture_data));
            variant = static_cast<uint>(scene.textures_1f.size() - 1);
          }
        } else {
          variant = texture_value;
        }
      };
      
      // Attempt to load all referred textures
      texture_load_3f(m.diffuse,   baseColorTexture.C_Str(), Colr { baseColorValue.r, baseColorValue.g, baseColorValue.b });
      texture_load_1f(m.metallic,  metallicTexture.C_Str(), metallicValue);
      texture_load_1f(m.roughness, roughnessTexture.C_Str(), roughnessValue);
      texture_load_1f(m.opacity,   opacityTexture.C_Str(), opacityValue);
      texture_load_3f(m.normals,   normalTexture.C_Str(), Colr { 0, 0, 1 });
      
      // Register material
      scene.materials.data().at(material_i_new) = { .name  = material->GetName().C_Str(), 
                                                    .value = std::move(m) };
    }

    import_scene(std::move(scene));
  }

  void SceneHandler::import_scene(const fs::path &path) {
    met_trace();
    import_scene(io::load_scene(path));
  }

  void SceneHandler::import_scene(Scene &&other) {
    // Import scene objects/emitters/materials/etc, taking care to increment indexes while bookkeeping correctly
    std::transform(range_iter(other.upliftings.data()), 
                   std::back_inserter(scene.upliftings.data()), [&](auto component) {
      for (auto &vert : component.value.verts) {
        vert.csys_i += scene.colr_systems.size();
        for (auto &j : vert.csys_j)
          j += scene.colr_systems.size();
        if (vert.type == Uplifting::Constraint::Type::eColorOnMesh) {
          vert.object_i += scene.objects.size();
        }
      }
      return component;
    });
    std::transform(range_iter(other.objects), std::back_inserter(scene.objects.data()), [&](auto component) {
      component.value.mesh_i      += scene.meshes.size();
      component.value.material_i  += scene.materials.size();
      if (!other.upliftings.empty())
        component.value.uplifting_i += scene.upliftings.size();
      return component;
    });
    std::transform(range_iter(other.emitters), std::back_inserter(scene.emitters.data()), [&](auto component) {
      if (!other.illuminants.empty())
        component.value.illuminant_i += scene.illuminants.size();
      return component;
    });
    std::transform(range_iter(other.materials), std::back_inserter(scene.materials.data()), [&](auto component) {
      if (component.value.diffuse.index() == 1)
        component.value.diffuse = static_cast<uint>(std::get<1>(component.value.diffuse) + scene.textures_3f.size());
      if (component.value.roughness.index() == 1)
        component.value.roughness = static_cast<uint>(std::get<1>(component.value.roughness) + scene.textures_1f.size());
      if (component.value.metallic.index() == 1)
        component.value.metallic = static_cast<uint>(std::get<1>(component.value.metallic) + scene.textures_1f.size());
      if (component.value.opacity.index() == 1)
        component.value.opacity = static_cast<uint>(std::get<1>(component.value.opacity) + scene.textures_1f.size());
      if (component.value.normals.index() == 1)
        component.value.normals = static_cast<uint>(std::get<1>(component.value.normals) + scene.textures_3f.size());
      return component;
    });
    std::transform(range_iter(other.colr_systems), std::back_inserter(scene.colr_systems.data()), [&](auto component) {
      if (!other.observers.empty())
        component.value.observer_i   += scene.observers.size();
      if (!other.illuminants.empty())
        component.value.illuminant_i += scene.illuminants.size();
      return component;
    });

    // Append scene resources from other scene behind current scene's components
    scene.meshes.data().insert(scene.meshes.end(),           std::make_move_iterator(other.meshes.begin()),
                                                             std::make_move_iterator(other.meshes.end()));
    scene.textures_3f.data().insert(scene.textures_3f.end(), std::make_move_iterator(other.textures_3f.begin()),
                                                             std::make_move_iterator(other.textures_3f.end()));
    scene.textures_1f.data().insert(scene.textures_1f.end(), std::make_move_iterator(other.textures_1f.begin()),
                                                             std::make_move_iterator(other.textures_1f.end()));
    scene.illuminants.data().insert(scene.illuminants.end(), std::make_move_iterator(other.illuminants.begin()),
                                                             std::make_move_iterator(other.illuminants.end()));
    scene.observers.data().insert(scene.observers.end(),     std::make_move_iterator(other.observers.begin()),
                                                             std::make_move_iterator(other.observers.end()));
    scene.bases.data().insert(scene.bases.end(),             std::make_move_iterator(other.bases.begin()),
                                                             std::make_move_iterator(other.bases.end()));
  }
} // namespace met