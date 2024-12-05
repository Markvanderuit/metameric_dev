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

#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/io.hpp>
#include <nlohmann/json_fwd.hpp>

namespace met {
  // namespace/typename shorthand inside met::io namespace
  using json = nlohmann::json; 

  namespace io {
    /* json load/save to/from file */
    json load_json(const fs::path &path);
    void save_json(const fs::path &path, const json &js, uint indent = 2);
  }

  /* json (de)serialization for BasisTreeNode type must be declared in met scope */
  void from_json(const json &js, Basis &b);
  void to_json(json &js, const Basis &b);

  /* json (de)serialization for Transform type */
  void from_json(const met::json &js, met::Transform &trf);
  void to_json(met::json &js, const met::Transform &trf);
} // namespace met

/* json (de)serializations for specific Eigen types must be declared in Eigen scope */
namespace Eigen { 
  void from_json(const met::json& js, Array2f &v);
  void to_json(met::json &js, const Array2f &v);

  void from_json(const met::json& js, Array2u &v);
  void to_json(met::json &js, const Array2u &v);

  void from_json(const met::json& js, Array3u &v);
  void to_json(met::json &js, const Array3u &v);

  void from_json(const met::json& js, Array4u &v);
  void to_json(met::json &js, const Array4u &v);

  void from_json(const met::json& js, met::Spec &v);
  void to_json(met::json &js, const met::Spec &v);
  
  void from_json(const met::json& js, met::Colr &v);
  void to_json(met::json &js, const met::Colr &v);
  
  void from_json(const met::json& js, met::CMFS &v);
  void to_json(met::json &js, const met::CMFS &v);

  void from_json(const met::json &js, met::Basis::mat_type &b);
  void to_json(met::json &js, const met::Basis::mat_type &b);

  void from_json(const met::json &js, Affine3f &t);
  void to_json(met::json &js, const Affine3f &t);

  // Basis loading
  void from_json(const met::json &js, Array<float, 31, 1> &m);
  void to_json(met::json &js, const Array<float, 31, 1> &m);
  void from_json(const met::json &js, Array<float, 31, 31> &m);
  void to_json(met::json &js, const Array<float, 31, 31> &m);
} // namespace Eigen