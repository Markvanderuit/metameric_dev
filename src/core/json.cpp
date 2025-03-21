// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <metameric/core/io.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/ranges.hpp>
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

  void from_json(const met::json &js, met::Basis &b) {
    b.scale = js.at("scale").get<float>();
    b.mean  = js.at("mean").get<Spec>();
    b.func  = js.at("func").get<met::Basis::mat_type>();
  }

  void to_json(met::json &js, const met::Basis &b) {
    js["scale"] = b.scale;
    js["mean"]  = b.mean;
    js["func"]  = b.func;
  }
  
  void from_json(const met::json &js, met::Transform &trf) {
    trf.position = js.at("position").get<Colr>();
    trf.rotation = js.at("rotation").get<Colr>();
    trf.scaling  = js.at("scaling").get<Colr>();
  }

  void to_json(met::json &js, const met::Transform &trf) {
    js["position"] = trf.position;
    js["rotation"] = trf.rotation;
    js["scaling"]  = trf.scaling;
  }
} // namespace met

namespace Eigen {
  void from_json(const met::json& js, Array2u &v) {
    std::ranges::copy(js, v.begin());
  }

  void from_json(const met::json& js, Array3u &v) {
    std::ranges::copy(js, v.begin());
  }
  
  void from_json(const met::json& js, Array4u &v) {
    std::ranges::copy(js, v.begin());
  }

  void to_json(met::json &js, const Array2u &v) {
    js = std::vector<Array2u::value_type>(v.begin(), v.end());
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
  
  void from_json(const met::json &js, met::Basis::mat_type &b) {
    std::ranges::copy(js, b.reshaped().begin());
  }

  void to_json(met::json &js, const met::Basis::mat_type &b) {
    auto r = b.reshaped();
    js = std::vector<met::Basis::mat_type::value_type>(r.begin(), r.end());
  }
  
  void from_json(const met::json& js, Array2f &v) {
    std::ranges::copy(js, v.begin());
  }

  void to_json(met::json &js, const Array2f &v) {
    js = std::vector<Array2f::value_type>(v.begin(), v.end());
  }
  
  void from_json(const met::json &js, Affine3f &t) {
    std::ranges::copy(js, t.matrix().reshaped().begin());
  }

  void to_json(met::json &js, const Affine3f &t) {
    auto r = t.matrix().reshaped();
    js = std::vector<Affine3f::Scalar>(r.begin(), r.end());
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