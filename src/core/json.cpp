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

  void from_json(const json &js, SpectralMapping &v) {
    v.cmfs       = js.at("cmfs").get<CMFS>();
    v.illuminant = js.at("illuminant").get<Spec>();
    v.n_scatters = js.at("n_scatters").get<uint>();
  }

  void to_json(json &js, const SpectralMapping &v) {
    js["cmfs"]       = v.cmfs;
    js["illuminant"] = v.illuminant;
    js["n_scatters"] = v.n_scatters;
  }

  void from_json(const json &js, ProjectData::Mapping &v) {
    v.cmfs       = js.at("cmfs").get<std::string>();
    v.illuminant = js.at("illuminant").get<std::string>();
    v.n_scatters = js.at("n_scatters").get<uint>();
  }

  void to_json(json &js, const ProjectData::Mapping &v) {
    js["cmfs"]       = v.cmfs;
    js["illuminant"] = v.illuminant;
    js["n_scatters"] = v.n_scatters;
  }

  void from_json(const json &js, ProjectData &v) {
    v.rgb_gamut   = js.at("rgb_gamut").get<std::array<Color, 4>>();
    v.spec_gamut  = js.at("spec_gamut").get<std::array<Spec, 4>>();
    v.mappings    = js.at("mappings").get<std::vector<std::pair<std::string, ProjectData::Mapping>>>();
    v.cmfs        = js.at("cmfs").get<std::vector<std::pair<std::string, CMFS>>>();
    v.illuminants = js.at("illuminants").get<std::vector<std::pair<std::string, Spec>>>();
  }

  void to_json(json &js, const ProjectData &v) {
    js["rgb_gamut"]   = v.rgb_gamut;
    js["spec_gamut"]  = v.spec_gamut;
    js["mappings"]    = v.mappings;
    js["cmfs"]        = v.cmfs;
    js["illuminants"] = v.illuminants;
  }
} // namespace met

namespace Eigen {
  void from_json(const met::json& js, met::Spec &v) {
    std::ranges::copy(js, v.begin());
  }

  void to_json(met::json &js, const met::Spec &v) {
    js = std::vector<met::Spec::value_type>(v.begin(), v.end());
  }
  
  void from_json(const met::json& js, met::Color &v) {
    std::ranges::copy(js, v.begin());
  }

  void to_json(met::json &js, const met::Color &v) {
    js = std::vector<met::Spec::value_type>(v.begin(), v.end());
  }
  
  void from_json(const met::json& js, met::CMFS &v) {
    std::ranges::copy(js, v.reshaped().begin());
  }

  void to_json(met::json &js, const met::CMFS &v) {
    auto r = v.reshaped();
    js = std::vector<met::Color::value_type>(r.begin(), r.end());
  }
} // namespace Eigen