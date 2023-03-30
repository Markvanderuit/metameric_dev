#include <metameric/core/io.hpp>
#include <metameric/core/json.hpp>
#include <nlohmann/json.hpp>

namespace met {
  namespace io {
    json load_json(const fs::path &path) {
      return json::parse(load_string(path));
    }

    void save_json(const fs::path &path, const json &js, uint indent) {
      save_string(path, js.dump(indent));
    }
  } // namespace io

  void from_json(const json &js, ProjectData::CSys &v) {
    v.cmfs       = js.at("cmfs").get<uint>();
    v.illuminant = js.at("illuminant").get<uint>();
    v.n_scatters = js.value<uint>("n_scatters", 1);
  }

  void to_json(json &js, const ProjectData::CSys &v) {
    js["cmfs"]       = v.cmfs;
    js["illuminant"] = v.illuminant;
    js["n_scatters"] = v.n_scatters;
  }

  void from_json(const json &js, ProjectData::Vert &v) {
    v.colr_i = js.at("colr_i").get<Colr>();
    v.csys_i = js.at("csys_i").get<uint>();
    v.colr_j = js.at("colr_j").get<std::vector<Colr>>();
    v.csys_j = js.at("csys_j").get<std::vector<uint>>();
  }

  void to_json(json &js, const ProjectData::Vert &v) {
    js["colr_i"] = v.colr_i;
    js["csys_i"] = v.csys_i;
    js["colr_j"] = v.colr_j;
    js["csys_j"] = v.csys_j;
  }
  
  void from_json(const json &js, BasisTreeNode &b) {
    // Extract structure data
    if (js.contains("children"))
      b.children = js.at("children").get<std::vector<BasisTreeNode>>();
    b.bbox_min = js.at("bbox_min").get<Chro>();
    b.bbox_max = js.at("bbox_max").get<Chro>();
    b.depth    = js.at("depth").get<uint>();

    // Extract node data
    constexpr uint data_wvls = 31;
    auto [wvls, mean, basis] = js.at("data").get<std::tuple<
      eig::Array<float, data_wvls, 1>, 
      eig::Array<float, data_wvls, 1>, 
      eig::Array<float, data_wvls, data_wvls>>
    >();

    // Format node data
    auto block = basis.eval().block<data_wvls, wavelength_bases>(0, 0).transpose().eval();
    // auto block = basis.transpose().eval().block<data_wvls, wavelength_bases>(0, 0).eval();
    for (uint i = 0; i < wavelength_bases; ++i) {
      b.basis.col(i) = io::spectrum_from_data(cnt_span<const float>(wvls), cnt_span<const float>(block.row(i).eval()), true);
    }
    b.basis_mean = io::spectrum_from_data(cnt_span<const float>(wvls), cnt_span<const float>(mean), true);
  }
  
  void to_json(json &js, const BasisTreeNode &b) {
    debug::check_expr(false, "Not imeplemented!");
  }

  void from_json(const json &js, ProjectData &v) {
    v.verts         = js.at("vertices").get<std::vector<ProjectData::Vert>>();
    v.color_systems = js.at("mappings").get<std::vector<ProjectData::CSys>>();
    v.cmfs          = js.at("cmfs").get<std::vector<std::pair<std::string, CMFS>>>();
    v.illuminants   = js.at("illuminants").get<std::vector<std::pair<std::string, Spec>>>();
    v.meshing_type  = ProjectMeshingType::eDelaunay; // TODO remove and fix
  }

  void to_json(json &js, const ProjectData &v) {
    js["vertices"]    = v.verts;
    js["mappings"]    = v.color_systems;
    js["cmfs"]        = v.cmfs;
    js["illuminants"] = v.illuminants;
  }
} // namespace met

namespace Eigen {
  void from_json(const met::json& js, Array3u &v) {
    std::ranges::copy(js, v.begin());
  }
  
  void from_json(const met::json& js, Array4u &v) {
    std::ranges::copy(js, v.begin());
  }

  void to_json(met::json &js, const Array3u &v) {
    js = std::vector<Array3u::value_type>(v.begin(), v.end());
  }

  void to_json(met::json &js, const Array4u &v) {
    js = std::vector<Array4u::value_type>(v.begin(), v.end());
  }

  void from_json(const met::json& js, met::Spec &v) {
    std::ranges::copy(js, v.begin());
  }

  void to_json(met::json &js, const met::Spec &v) {
    js = std::vector<met::Spec::value_type>(v.begin(), v.end());
  }
  
  void from_json(const met::json& js, met::Colr &v) {
    std::ranges::copy(js, v.begin());
  }

  void to_json(met::json &js, const met::Colr &v) {
    js = std::vector<met::Colr::value_type>(v.begin(), v.end());
  }
  
  void from_json(const met::json& js, met::CMFS &v) {
    std::ranges::copy(js, v.reshaped().begin());
  }

  void to_json(met::json &js, const met::CMFS &v) {
    auto r = v.reshaped();
    js = std::vector<met::CMFS::value_type>(r.begin(), r.end());
  }
  
  void from_json(const met::json &js, met::Basis &b) {
    std::ranges::copy(js, b.reshaped().begin());
  }

  void to_json(met::json &js, const met::Basis &b) {
    auto r = b.reshaped();
    js = std::vector<met::Basis::value_type>(r.begin(), r.end());
  }
  
  void from_json(const met::json& js, met::Chro &v) {
    std::ranges::copy(js, v.begin());
  }

  void to_json(met::json &js, const met::Chro &v) {
    js = std::vector<met::Chro::value_type>(v.begin(), v.end());
  }

  void from_json(const met::json &js, Array<float, 31, 1> &m) {
    std::ranges::copy(js, m.begin());
  }

  void to_json(met::json &js, const Array<float, 31, 1> &m) {
    js = std::vector<float>(m.begin(), m.end());
  }

  void from_json(const met::json &js, Array<float, 31, 31> &m) {
    std::ranges::copy(js, m.reshaped().begin());
  }

  void to_json(met::json &js, const Array<float, 31, 31> &m) {
    auto r = m.reshaped();
    js = std::vector<float>(r.begin(), r.end());
  }
} // namespace Eigen