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

  void from_json(const json &js, ProjectData::Mapp &v) {
    v.cmfs       = js.at("cmfs").get<std::string>();
    v.illuminant = js.at("illuminant").get<std::string>();
    v.n_scatters = js.at("n_scatters").get<uint>();
  }

  void to_json(json &js, const ProjectData::Mapp &v) {
    js["cmfs"]       = v.cmfs;
    js["illuminant"] = v.illuminant;
    js["n_scatters"] = v.n_scatters;
  }

  void from_json(const json &js, ProjectData &v) {
    v.gamut_elems  = js.at("gamut_elems").get<std::vector<eig::Array3u>>();
    v.gamut_colr_i = js.at("gamut_colr_i").get<std::vector<Colr>>();
    v.gamut_offs_j = js.at("gamut_offs_j").get<std::vector<Colr>>();
    v.gamut_mapp_i = js.at("gamut_mapp_i").get<std::vector<uint>>();
    v.gamut_mapp_j = js.at("gamut_mapp_j").get<std::vector<uint>>();
    v.mappings     = js.at("mappings").get<std::vector<std::pair<std::string, ProjectData::Mapp>>>();
    v.cmfs         = js.at("cmfs").get<std::vector<std::pair<std::string, CMFS>>>();
    v.illuminants  = js.at("illuminants").get<std::vector<std::pair<std::string, Spec>>>();
  }

  void to_json(json &js, const ProjectData &v) {
    js["gamut_elems"]  = v.gamut_elems;
    js["gamut_colr_i"] = v.gamut_colr_i;
    js["gamut_offs_j"] = v.gamut_offs_j;
    js["gamut_mapp_i"] = v.gamut_mapp_i;
    js["gamut_mapp_j"] = v.gamut_mapp_j;
    js["mappings"]     = v.mappings;
    js["cmfs"]         = v.cmfs;
    js["illuminants"]  = v.illuminants;
  }
} // namespace met

namespace Eigen {
  void from_json(const met::json& js, Array3u &v) {
    std::ranges::copy(js, v.begin());
  }

  void to_json(met::json &js, const Array3u &v) {
    js = std::vector<Array3u::value_type>(v.begin(), v.end());
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
} // namespace Eigen