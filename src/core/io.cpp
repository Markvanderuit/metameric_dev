#include <metameric/core/io.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <nlohmann/json.hpp>
#include <zstr.hpp>
#include <algorithm>
#include <execution>
#include <functional>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <deque>

namespace met::io {
  using namespace std::placeholders;

  std::string load_string(const fs::path &path) {
    met_trace();

    // Check that file path exists
    debug::check_expr(fs::exists(path),
      fmt::format("failed to resolve path \"{}\"", path.string()));
      
    // Attempt to open file stream
    std::ifstream ifs(path, std::ios::ate);
    debug::check_expr(ifs.is_open(),
      fmt::format("failed to open file \"{}\"", path.string()));
      
    // Read file size and construct string to hold data
    size_t file_size = static_cast<size_t>(ifs.tellg());
    std::string str(file_size, ' ');

    // Set input position to start, then read full file into buffer
    ifs.seekg(0);
    ifs.read((char *) str.data(), file_size);
    ifs.close();

    return str;
  }

  void save_string(const fs::path &path, const std::string &str) {
    met_trace();
    
    // Attempt to open output file stream in text mode
    std::ofstream ofs(path, std::ios::out);
    debug::check_expr(ofs.is_open(),
      fmt::format("failed to open file \"{}\"", path.string()));

    // Write string directly to file in text mode
    ofs.write(str.data(), str.size());
    ofs.close();
  }

  Spec load_spec(const fs::path &path) {
    met_trace();

    // Output data blocks
    std::vector<float> wvls, values;

    // Read spectrum file as string, and parse line by line
    std::stringstream ss(load_string(path));
    std::string line;
    while (std::getline(ss, line)) {
      std::ranges::replace(line, '\t', ' ');
      auto split_vect = line 
                      | std::views::split(' ') 
                      | std::views::transform([](auto &&r) { return std::string(r.begin(), r.end()); })
                      | std::ranges::to<std::vector>();

      // Skip empty and commented lines
      guard_continue(!split_vect.empty() && split_vect[0][0] != '#');

      wvls.push_back(std::stof(split_vect[0]));
      values.push_back(std::stof(split_vect[1]));
    }

    return spectrum_from_data(wvls, values);
  }

  void save_spec(const fs::path &path, const Spec &s) {
    met_trace();

    // Get spectral data split into blocks
    auto [wvls, values] = spectrum_to_data(s);

    // Parse split data into string format
    std::stringstream ss;
    for (uint i = 0; i < wvls.size(); ++i) {
      ss << fmt::format("{:.6f} {:.6f}", wvls[i], values[i]);
      if (i < wvls.size() - 1)
        ss << '\n';
    }

    return save_string(path, ss.str());
  }

  CMFS load_cmfs(const fs::path &path) {
    met_trace();

    // Output data blocks
    std::vector<float> wvls, values_x, values_y, values_z;

    // Read spectrum file as string into stringstream
    std::stringstream ss(load_string(path));

    // Parse line by line
    std::string line;
    uint        line_nr = 0;
    while (std::getline(ss, line)) {
      auto split_vect = line 
                      | std::views::split(' ') 
                      | std::views::transform([](auto &&r) { return std::string(r.begin(), r.end()); })
                      | std::ranges::to<std::vector>();

      // Skip empty and commented lines
      guard_continue(!split_vect.empty() && split_vect[0][0] != '#');

      // Throw on incorrect input data 
      debug::check_expr(split_vect.size() == 4,
        fmt::format("CMFS data incorrect on line {}\n", line_nr));

      wvls.push_back(std::stof(split_vect[0]));
      values_x.push_back(std::stof(split_vect[1]));
      values_y.push_back(std::stof(split_vect[2]));
      values_z.push_back(std::stof(split_vect[3]));
      
      line_nr++;
    }

    return cmfs_from_data(wvls, values_x, values_y, values_z);
  }

  void save_cmfs(const fs::path &path, const CMFS &s) {
    met_trace();

    // Get spectral data split into blocks
    auto [wvls, values_x, values_y, values_z] = cmfs_to_data(s);

    // Parse split data into string format
    std::stringstream ss;
    for (uint i = 0; i < wvls.size(); ++i)
      ss << fmt::format("{:.6f} {:.6f} {:.6f} {:.6f}\n", wvls[i], values_x[i], values_y[i], values_z[i]);

    return save_string(path, ss.str());
  }

  template <typename Mesh>
  Mesh load_mesh(const fs::path &path) {
    met_trace();

    Assimp::Importer imp;
    const auto *scene = imp.ReadFile(path.string(), 
      aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals);
    
    debug::check_expr(scene/*  || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE */,
      fmt::format("Could not load scnene data from {}. {}\n", 
                  path.string(),
                  std::string(imp.GetErrorString())));

    {
      struct QueueObject {
        eig::Matrix4f trf;
        aiNode       *node;
      } root = { eig::Matrix4f::Identity(), scene->mRootNode };

      std::unordered_set<uint> meshes, textures;

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
          
        }
        
        // Push child nodes on work queue
        for (auto child : std::span { node->mChildren, node->mNumChildren })
          queue.push_back({ trf, child });
      }
    }


    // For now, just load the base mesh
    // TODO; actually handle scenes, not individual meshes
    const auto *mesh = scene->mMeshes[0];

    fmt::print("Meshes : {}\nMaterials : {}\nTextures : {}\n",
      scene->mNumMeshes, scene->mNumMaterials, scene->mNumTextures);
    
    std::span materials = { scene->mMaterials, scene->mNumMaterials };
    for (auto *mat : materials) {
      std::string mat_name = mat->GetName().C_Str();
      fmt::print("{}\n", mat_name);

      fmt::print("Texture keys >\n", mat_name);
      {
        aiString baseColorTexture, metallicTexture, roughnessTexture, opacityTexture, normalTexture;

        // Search for a corresponding texture value for diffuse
        for (auto tag : { aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE }) {
          mat->GetTexture(tag, 0, &baseColorTexture);
          guard_break(baseColorTexture.length == 0);
        }
        
        // Search for a corresponding texture value for normal maps
        for (auto tag : { aiTextureType_NORMALS, aiTextureType_HEIGHT, aiTextureType_NORMAL_CAMERA }) {
          mat->GetTexture(tag, 0, &normalTexture);
          guard_break(normalTexture.length == 0);
        }

        // Gather texture paths for miscellaneous values
        mat->GetTexture(aiTextureType_METALNESS,         0, &metallicTexture);
        mat->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &roughnessTexture);
        mat->GetTexture(aiTextureType_OPACITY,           0, &opacityTexture);

        fmt::print("Base color {}\n", baseColorTexture.C_Str());
        fmt::print("Metallic   {}\n", metallicTexture.C_Str());
        fmt::print("Roughness  {}\n", roughnessTexture.C_Str());
        fmt::print("Opacity    {}\n", opacityTexture.C_Str());
        fmt::print("Normals    {}\n", normalTexture.C_Str());
      }

      fmt::print("Properties >\n", mat_name);
      std::span properties = { mat->mProperties, mat->mNumProperties };
      for (auto *prop : properties) {
        std::string prop_name = prop->mKey.C_Str();
        uint prop_semantic = prop->mSemantic;
        std::string buffer(prop->mData, size_t(prop->mDataLength));

        switch (prop->mType) {
          case aiPTI_Float:
            fmt::print("\t{} - F32 - {}\n", prop_name, "" /* mat->Get(prop->mKey, aiPTI_Float, ) */);
            break;
          case aiPTI_Double:
            fmt::print("\t{} - F64 - {}\n", prop_name, "");
            break;
          case aiPTI_Integer:
            fmt::print("\t{} - I32 - {}\n", prop_name, "");
            break;
          case aiPTI_String:
            fmt::print("\t{} - Str - {} - {}\n", prop_name, buffer, prop_semantic);
            break;
          case aiPTI_Buffer:
            fmt::print("\t{} - Buf - {}\n", prop_name, "");
            break;
        }
      }
    }

    AlMesh m;
    
    if (mesh->HasPositions()) {
      std::span verts = { mesh->mVertices, mesh->mNumVertices };
      m.verts.resize(verts.size());
      std::transform(std::execution::par_unseq, range_iter(verts), m.verts.begin(),
        [](const auto &v) { return AlMesh::vert_type { v.x, v.y, v.z }; });
    }
    
    if (mesh->HasNormals()) {
      std::span norms = { mesh->mNormals, mesh->mNumVertices };
      m.norms.resize(norms.size());
      std::transform(std::execution::par_unseq, range_iter(norms), m.norms.begin(),
        [](const auto &v) { return AlMesh::norm_type { v.x, v.y, v.z }; });
    }

    // Assume first set of coords only
    constexpr size_t default_texture_coord = 0;
    if (mesh->HasTextureCoords(default_texture_coord)) {
      std::span txuvs = { mesh->mTextureCoords[default_texture_coord], mesh->mNumVertices };
      m.txuvs.resize(txuvs.size());
      std::transform(std::execution::par_unseq, range_iter(txuvs), m.txuvs.begin(),
        [](const auto &v) { return AlMesh::txuv_type { v.x, v.y }; });
    }

    if (mesh->HasFaces()) {
      std::span elems = { mesh->mFaces, mesh->mNumFaces };
      m.elems.resize(elems.size());
      std::transform(std::execution::par_unseq, range_iter(elems), m.elems.begin(),
        [](const aiFace &v) { return AlMesh::elem_type { v.mIndices[0], v.mIndices[1], v.mIndices[2] }; });
    }
    
    // Ensure mesh data is properly corrected and redundant vertices are stripped
    remap_mesh(m);
    compact_mesh(m);
    
    return convert_mesh<Mesh>(m);
  }

  Basis load_basis(const fs::path &path) {
    met_trace();
    
    // Lambda shorthand for overload of std::stof
    constexpr auto stof = [](const std::string &s) -> float { return std::stof(s); };

    // Different read modes while parsing the file; by default, no read is performed
    enum class read_mode {  eNone, eMean, eFunc } mode = read_mode::eNone;

    // Output data blocks; 
    // we later generate spectrum and basis data for the given wavelength/signal
    std::vector<float>           wvls_mean, 
                                 sgnl_mean,
                                 wvls_func;
    std::vector<Basis::vec_type> sgnl_func;

    // Read spectrum file as string
    std::stringstream ss(load_string(path));

    // Parse string line by line
    std::string line;
    uint        line_nr = 0;
    while (std::getline(ss, line)) {
      // Test for mean/func heading comments, setting mean/func read mode
      if (line == "# mean") {
        line_nr++;
        mode = read_mode::eMean;
        continue;
      } else if (line == "# func") {
        line_nr++;
        mode = read_mode::eFunc;
        continue;
      } else if (mode == read_mode::eNone) {
        line_nr++;
        continue;
      }

      // Separate current line by spaces
      auto split = line 
                 | vws::split(' ') 
                 | vws::transform([](auto &&r) { return std::string(range_iter(r)); })
                 | rng::to<std::vector>();
      auto data = std::span(split); // span representation for slicing

      // Skip empty or commented lines
      guard_continue(!data.empty() && data[0][0] != '#');

      if (mode == read_mode::eMean) {
        debug::check_expr(data.size() >= 2,
          fmt::format("Basis mean data too short on line {}\n", line_nr));
        
        // String-to-float the first two values, as wavelength and 
        // corresponding signal
        wvls_mean.push_back(stof(data[0]));
        sgnl_mean.push_back(stof(data[1]));
      } else if (mode == read_mode::eFunc) {
        // Throw on incorrect input data length
        debug::check_expr(data.size() >= wavelength_bases + 1,
          fmt::format("Basis func data too short on line {}\n", line_nr));
        
        // String-to-float the first value, which describes wavelength, 
        // and the following N characters, which describe 
        Basis::vec_type signal;
        rng::transform(data.subspan(1, wavelength_bases), signal.begin(), stof);
        wvls_func.push_back(stof(data[0]));
        sgnl_func.push_back(signal);
      }

      line_nr++;
    }

    return basis_from_data(wvls_mean, sgnl_mean, wvls_func, sgnl_func);
  }

  void save_spectral_data(const SpectralData &data, const fs::path &path) {
    met_trace();
    
    // Attempt to open output file stream in binary mode using zlib stream wrapper
    zstr::ofstream ofs(path.string(), std::ios::out | std::ios::trunc | std::ios::binary,  -1);

    // Output header data
    std::array<float, 2> header_f = { data.spec_min, data.spec_max };
    std::array<uint, 4>  header_u = { data.spec_samples, data.bary_xres, data.bary_yres, data.bary_zres };
    ofs.write((const char *) header_f.data(), header_f.size() * sizeof(decltype(header_f)::value_type));
    ofs.write((const char *) header_u.data(), header_u.size() * sizeof(decltype(header_u)::value_type));

    // Output bulk data
    const size_t functions_size = data.functions.size() * sizeof(decltype(data.functions)::value_type);
    const size_t weights_size   = data.weights.size() * sizeof(decltype(data.weights)::value_type);
    ofs.write((const char *) data.functions.data(), functions_size);
    ofs.write((const char *) data.weights.data(), weights_size);

    // Flush only; ofs closes on destruction
    ofs.flush();
  }

  // Src: Mitsuba 0.5, reimplements InterpolatedSpectrum::average(...) from libcore/spectrum.cpp
  Spec spectrum_from_data(std::span<const float> wvls, std::span<const float> values, bool remap) {
    met_trace();

    // Generate extended wavelengths for now, fitting current spectral range
    /* std::vector<float> wvls;
    if (remap) {
      wvls.resize(values.size());
      for (int i = 0; i < wvls.size(); ++i) {
        wvls[i] = wavelength_min + i * (wavelength_range / static_cast<float>(values.size() - 1));
      }
    } else {
      wvls = std::vector<float>(range_iter(wvls_));
    } */

    Spec s = 0.f;

    float data_wvl_min = wvls.front(), data_wvl_max = wvls.back();

    for (size_t i = 0; i < wavelength_samples; ++i) {
      float spec_wvl_min = wavelength_min + i * wavelength_ssize,
            spec_wvl_max = spec_wvl_min + wavelength_ssize;

      // Determine accessible range of wavelengths
      float wvl_min = std::max(spec_wvl_min, data_wvl_min),
            wvl_max = std::min(spec_wvl_max, data_wvl_max);
      guard_continue(wvl_max > wvl_min);

      // Find the starting index using binary search (Thanks for the idea, Mitsuba people!)
      ptrdiff_t pos = std::max(std::ranges::lower_bound(wvls, wvl_min) - wvls.begin(),
                              static_cast<ptrdiff_t>(1)) - 1;
      
      // Step through the provided data and integrate trapezoids
      for (; pos + 1 < wvls.size() && wvls[pos] < wvl_max; ++pos) {
        float wvl_a   = wvls[pos],
              value_a = values[pos],
              clamp_a = std::max(wvl_a, wvl_min);
        float wvl_b   = wvls[pos + 1],
              value_b = values[pos + 1],
              clamp_b = std::min(wvl_b, wvl_max);
        guard_continue(clamp_b > clamp_a);

        float inv_ab = 1.f / (wvl_b - wvl_a);
        float interp_a = std::lerp(value_a, value_b, (clamp_a - wvl_a) * inv_ab),
              interp_b = std::lerp(value_a, value_b, (clamp_b - wvl_a) * inv_ab);

        s[i] += .5f * (interp_a + interp_b) * (clamp_b - clamp_a);
      }
      s[i] /= wavelength_ssize;
    }

    return s.eval();
  }
 
  CMFS cmfs_from_data(std::span<const float> wvls, std::span<const float> values_x,
                      std::span<const float> values_y, std::span<const float> values_z) {
    met_trace();
    return (CMFS() << spectrum_from_data(wvls, values_x),
                      spectrum_from_data(wvls, values_y),
                      spectrum_from_data(wvls, values_z)).finished();
  }

  Basis basis_from_data(std::span<const float>     wvls_mean, 
                        std::span<const float>     sgnl_mean, 
                        std::span<const float>     wvls_func, 
                        std::span<Basis::vec_type> sgnl_func) {
    met_trace();

    // Output data
    Basis s = {
      .mean = spectrum_from_data(wvls_mean, sgnl_mean),
      .func = 0.f
    };
    s.scale = 1.f / std::max(s.mean.array().maxCoeff(), std::abs(s.mean.array().minCoeff()));

    float data_wvl_min = wvls_func.front(), data_wvl_max = wvls_func.back();

    for (size_t j = 0; j < wavelength_bases; ++j) {
      for (size_t i = 0; i < wavelength_samples; ++i) {
        float spec_wvl_min = i * wavelength_ssize + wavelength_min,
              spec_wvl_max = spec_wvl_min + wavelength_ssize;

        // Determine accessible range of wavelengths
        float wvl_min = std::max(spec_wvl_min, data_wvl_min),
              wvl_max = std::min(spec_wvl_max, data_wvl_max);
        guard_continue(wvl_max > wvl_min);

        // Find the starting index using binary search (Thanks for the idea, Mitsuba people!)
        ptrdiff_t pos = std::max(std::ranges::lower_bound(wvls_func, wvl_min) - wvls_func.begin(),
                                static_cast<ptrdiff_t>(1)) - 1;
        
        // Step through the provided data and integrate trapezoids
        for (; pos + 1 < wvls_func.size() && wvls_func[pos] < wvl_max; ++pos) {
          float wvl_a   = wvls_func[pos],
                value_a = sgnl_func[pos][j],
                clamp_a = std::max(wvl_a, wvl_min);
          float wvl_b   = wvls_func[pos + 1],
                value_b = sgnl_func[pos + 1][j],
                clamp_b = std::min(wvl_b, wvl_max);
          guard_continue(clamp_b > clamp_a);

          float inv_ab = 1.f / (wvl_b - wvl_a);
          float interp_a = std::lerp(value_a, value_b, (clamp_a - wvl_a) * inv_ab),
                interp_b = std::lerp(value_a, value_b, (clamp_b - wvl_a) * inv_ab);

          s.func(i, j) += .5f * (interp_a + interp_b) * (clamp_b - clamp_a);
        }
        s.func(i, j) /= wavelength_ssize;
      }
    }

    return s;
  }
  
  std::array<std::vector<float>, 2> spectrum_to_data(const Spec &s) {
    std::vector<float> wvls(wavelength_samples);
    std::vector<float> values(wavelength_samples);

    std::ranges::transform(std::views::iota(0u, wavelength_samples), wvls.begin(), wavelength_at_index);
    std::ranges::copy(s, values.begin());

    return { wvls, values };
  }

  std::array<std::vector<float>, 4> cmfs_to_data(const CMFS &s)  {
    std::vector<float> wvls(wavelength_samples);
    std::vector<float> values_x(wavelength_samples), 
      values_y(wavelength_samples), values_z(wavelength_samples);
    
    std::ranges::transform(std::views::iota(0u, wavelength_samples), wvls.begin(), wavelength_at_index);
    std::ranges::copy(s.col(0), values_x.begin());
    std::ranges::copy(s.col(1), values_y.begin());
    std::ranges::copy(s.col(2), values_z.begin());
    
    return { wvls, values_x, values_y, values_z };
  }

  /* Explicit template instantiations */

  template
  Mesh load_mesh<Mesh>(const fs::path &);
  template
  AlMesh load_mesh<AlMesh>(const fs::path &);
} // namespace met::io