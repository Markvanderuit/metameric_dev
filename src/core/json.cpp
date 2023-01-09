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
  }

  void to_json(json &js, const ProjectData::CSys &v) {
    js["cmfs"]       = v.cmfs;
    js["illuminant"] = v.illuminant;
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

  void from_json(const json &js, ProjectData &v) {
    v.gamut_elems  = js.at("gamut_elems").get<std::vector<ProjectData::Elem>>();
    v.gamut_verts  = js.at("gamut_verts").get<std::vector<ProjectData::Vert>>();
    v.sample_verts = js.at("sample_verts").get<std::vector<ProjectData::Vert>>();
    v.color_systems     = js.at("mappings").get<std::vector<ProjectData::CSys>>();
    v.cmfs         = js.at("cmfs").get<std::vector<std::pair<std::string, CMFS>>>();
    v.illuminants  = js.at("illuminants").get<std::vector<std::pair<std::string, Spec>>>();
  }

  void to_json(json &js, const ProjectData &v) {
    js["gamut_elems"]  = v.gamut_elems;
    js["gamut_verts"]  = v.gamut_verts;
    js["sample_verts"] = v.sample_verts;
    js["mappings"]     = v.color_systems;
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