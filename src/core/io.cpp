#include <metameric/core/io.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/scene/scene.hpp>
#include <metameric/core/utility.hpp>
#include <nlohmann/json.hpp>
#include <rapidobj/rapidobj.hpp>
#include <algorithm>
#include <deque>
#include <execution>
#include <functional>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

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
  
  Scene load_obj(const fs::path &obj_path, bool load_materials, bool flip_uvs) {
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

    // Return object; create (empty) scene to store objects/meshes/textures for output
    Scene scene;

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
      }

      // 3 - create an object component referring to mesh/texture
      met::Object object = {
        .mesh_i      = static_cast<uint>(scene.resources.meshes.size()),
        .uplifting_i = 0,
        .brdf_type   = brdf_type,
        .diffuse     = diffuse,
        .metallic    = metallic,
        .roughness   = roughness
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

    return scene;
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
                 | vws::take(1 + wavelength_bases)
                 | vws::transform([](auto &&r) { return std::string(range_iter(r)); })
                 | rng::to<std::vector>();
      auto data = std::span(split); // span representation for slicing

      // Skip empty or commented lines
      guard_continue(!data.empty() && data[0][0] != '#' && !data[0].empty());

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


    // TODO; REMOVE HARDCODED FOR PAPER TEST
    eig::Vector<float, 32> ev = {
      4.80323533e+01f, 7.51669501e+00f, 4.21518090e+00f, 2.06736524e+00f,
      1.12826738e+00f, 3.10498058e-01f, 2.64876889e-01f, 1.43566338e-01f,
      6.96868703e-02f, 5.60114977e-02f, 3.38647589e-02f, 2.67549622e-02f,
      2.32778257e-02f, 2.04676939e-02f, 1.59482759e-02f, 1.11668831e-02f,
      1.06935859e-02f, 8.98293044e-03f, 7.15868075e-03f, 5.56392880e-03f,
      3.88411047e-03f, 3.47654084e-03f, 3.39976034e-03f, 2.63938959e-03f,
      2.33516443e-03f, 1.97400732e-03f, 1.75606828e-03f, 1.56239373e-03f,
      1.40881035e-03f, 1.26965006e-03f, 1.08290781e-03f, 9.45006468e-04f, 
    }; 
    
    uint i = 0;
    for (auto col : s.func.colwise()) {
      // Ensure EVe is normal (should be the case), then scale by EVa;
      col.normalize();
      col *= ev[i]; 
      
      // finally, scale to [-1, 1] boundary
      col /= std::max(std::abs(col.minCoeff()), std::abs(col.maxCoeff()));

      i++;
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
} // namespace met::io