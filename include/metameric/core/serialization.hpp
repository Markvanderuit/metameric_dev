#pragma once

#include <metameric/core/project.hpp>
#include <metameric/core/spectrum.hpp>
#include <nlohmann/json.hpp>
#include <string.h>
#include <ranges>

namespace met {
  using json = nlohmann::json; // namespace/typename shorthand
} // namespace met

namespace Eigen { /* json (de)serializations for specific Eigen types */
  using json = met::json;

  inline
  void from_json(const json& stream, met::Spec &v) {
    std::ranges::copy(stream, v.begin());
  }

  inline
  void to_json(json &stream, const met::Spec &v) {
    stream = std::vector<met::Spec::value_type>(v.begin(), v.end());
  }

  inline
  void from_json(const json& stream, met::Color &v) {
    std::ranges::copy(stream, v.begin());
  }

  inline
  void to_json(json &stream, const met::Color &v) {
    stream = std::vector<met::Color::value_type>(v.begin(), v.end());
  }

  inline
  void from_json(const json& stream, met::CMFS &v) {
    std::ranges::copy(stream, v.reshaped().begin());
  }

  inline
  void to_json(json &stream, const met::CMFS &v) {
    auto r = v.reshaped();
    stream = std::vector<met::Color::value_type>(r.begin(), r.end());
  }
} // namespace Eigen

namespace met { /* json (de)serializations for specific metameric types */
  inline
  void from_json(const json &stream, SpectralMapping &m) {
    m.cmfs       = stream.at("cmfs").get<CMFS>();
    m.illuminant = stream.at("illuminant").get<Spec>();
    m.n_scatters = stream.at("n_scatters").get<uint>();
  }

  inline
  void to_json(json &stream, const SpectralMapping &m) {
    stream["cmfs"]       = m.cmfs;
    stream["illuminant"] = m.illuminant;
    stream["n_scatters"] = m.n_scatters;
  }

  inline
  void from_json(const json &stream, Project &p) {
    p.texture_cache_path  = stream.at("texture_cache_path").get<std::string>();
    p.database_cache_path = stream.at("database_cache_path").get<std::string>();
    p.rgb_mapping         = stream.at("rgb_mapping").get<SpectralMapping>();
    p.rgb_gamut           = stream.at("rgb_mapping").get<std::array<Color, 4>>();
    p.spectral_mappings   = stream.at("spectral_mappings").get<std::unordered_map<std::string, SpectralMapping>>();
    p.loaded_cmfs         = stream.at("loaded_cmfs").get<std::unordered_map<std::string, CMFS>>();
    p.loaded_illuminants  = stream.at("loaded_illuminants").get<std::unordered_map<std::string, Spec>>();
  }

  inline
  void to_json(json &stream, const Project &p) {
    stream["texture_cache_path"]  = p.texture_cache_path.string();
    stream["database_cache_path"] = p.database_cache_path.string();
    stream["rgb_mapping"]         = p.rgb_mapping;
    stream["rgb_gamut"]           = p.rgb_gamut;
    stream["spectral_mappings"]   = p.spectral_mappings;
    stream["loaded_cmfs"]         = p.loaded_cmfs;
    stream["loaded_illuminants"]  = p.loaded_illuminants;
  }
} // namespace met