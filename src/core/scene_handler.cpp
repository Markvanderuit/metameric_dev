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
    scene.observer_i = { .name = "Default observer", .value = 0 };
    scene.resources.bases.emplace("Default basis", {
      .mean      = loaded_tree.basis_mean, .functions = loaded_tree.basis
    });
    scene.resources.illuminants.push("D65",      models::emitter_cie_d65);
    scene.resources.illuminants.push("E",        models::emitter_cie_e);
    scene.resources.illuminants.push("FL2",      models::emitter_cie_fl2);
    scene.resources.illuminants.push("FL11",     models::emitter_cie_fl11);
    scene.resources.illuminants.push("LED-RGB1", models::emitter_cie_ledrgb1);
    scene.resources.observers.push("CIE XYZ",    models::cmfs_cie_xyz);
    scene.components.upliftings.emplace("Default uplifting", {
      .type    = Uplifting::Type::eDelaunay,
      .basis_i = 0
    });
    Scene::ColrSystem csys { .observer_i = 0, .illuminant_i = 0, .n_scatters = 0 };
    scene.components.colr_systems.push(scene.get_csys_name(csys), csys);
    scene.components.emitters.push("Default D65 emitter", {
      .p            = { 0, 1, 0 },
      .multiplier   = 1.f,
      .illuminant_i = 0
    });
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

        // If current node has meshes attached, register object(s)
        for (uint i : std::span { node->mMeshes, node->mNumMeshes }) {
          // Register mesh material if not yet registered
          uint material_i = file->mMeshes[i]->mMaterialIndex;
          if (!material_uuid.contains(material_i))
            material_uuid[material_i] = material_uuid.size();
          material_i = material_uuid[material_i];

          scene.components.objects.emplace(node->mName.C_Str(), {
            .mesh_i      = i,
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

      uint tx_count = 0;
      for (uint i = 0; i < 16; ++i)
        tx_count += mesh->HasTextureCoords(i) ? 1 : 0;
      fmt::print("num texture coords; {}\n", tx_count);
      
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
      image_load(object.metallic,  metallicTexture.C_Str(), metallicValue);
      image_load(object.roughness, roughnessTexture.C_Str(), roughnessValue);
      image_load(object.opacity,   opacityTexture.C_Str(), opacityValue);
      image_load(object.normals,   normalTexture.C_Str(), Colr { 0, 0, 1 });
    }

    import_scene(std::move(scene));
  }

  void SceneHandler::import_scene(const fs::path &path) {
    met_trace();
    import_scene(io::load_scene(path));
  }

  void SceneHandler::import_scene(Scene &&other) {
    // Import scene objects/emitters/materials/etc, taking care to increment indexes while bookkeeping correctly
    std::transform(range_iter(other.components.upliftings), 
                   std::back_inserter(scene.components.upliftings.data()), [&](auto component) {
      for (auto &vert : component.value.verts) {
        vert.csys_i += scene.components.colr_systems.size();
        for (auto &j : vert.csys_j)
          j += scene.components.colr_systems.size();
        if (vert.type == Uplifting::Constraint::Type::eColorOnMesh) {
          vert.object_i += scene.components.objects.size();
        }
      }
      return component;
    });
    std::transform(range_iter(other.components.objects), 
                   std::back_inserter(scene.components.objects.data()), [&](auto component) {
      component.value.mesh_i += scene.resources.meshes.size();
      if (component.value.diffuse.index() == 1)
        component.value.diffuse = static_cast<uint>(std::get<1>(component.value.diffuse) + scene.resources.images.size());
      if (component.value.roughness.index() == 1)
        component.value.roughness = static_cast<uint>(std::get<1>(component.value.roughness) + scene.resources.images.size());
      if (component.value.metallic.index() == 1)
        component.value.metallic = static_cast<uint>(std::get<1>(component.value.metallic) + scene.resources.images.size());
      if (component.value.opacity.index() == 1)
        component.value.opacity = static_cast<uint>(std::get<1>(component.value.opacity) + scene.resources.images.size());
      if (component.value.normals.index() == 1)
        component.value.normals = static_cast<uint>(std::get<1>(component.value.normals) + scene.resources.images.size());
      if (!other.components.upliftings.empty())
        component.value.uplifting_i += scene.components.upliftings.size();
      return component;
    });
    std::transform(range_iter(other.components.emitters), 
                   std::back_inserter(scene.components.emitters.data()), [&](auto component) {
      if (!other.resources.illuminants.empty())
        component.value.illuminant_i += scene.resources.illuminants.size();
      return component;
    });
    std::transform(range_iter(other.components.colr_systems), 
                   std::back_inserter(scene.components.colr_systems.data()), [&](auto component) {
      if (!other.resources.observers.empty())
        component.value.observer_i   += scene.resources.observers.size();
      if (!other.resources.illuminants.empty())
        component.value.illuminant_i += scene.resources.illuminants.size();
      return component;
    });

    // Append scene resources from other scene behind current scene's components
    scene.resources.meshes.data().insert(scene.resources.meshes.end(),           
      std::make_move_iterator(other.resources.meshes.begin()),
      std::make_move_iterator(other.resources.meshes.end()));
    scene.resources.images.data().insert(scene.resources.images.end(),           
      std::make_move_iterator(other.resources.images.begin()),
      std::make_move_iterator(other.resources.images.end()));
    scene.resources.illuminants.data().insert(scene.resources.illuminants.end(), 
      std::make_move_iterator(other.resources.illuminants.begin()),
      std::make_move_iterator(other.resources.illuminants.end()));
    scene.resources.observers.data().insert(scene.resources.observers.end(),     
      std::make_move_iterator(other.resources.observers.begin()),
      std::make_move_iterator(other.resources.observers.end()));
    scene.resources.bases.data().insert(scene.resources.bases.end(),             
      std::make_move_iterator(other.resources.bases.begin()),
      std::make_move_iterator(other.resources.bases.end()));
  }
} // namespace met