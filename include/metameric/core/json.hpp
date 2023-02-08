#pragma once

#include <metameric/core/data.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/tree.hpp>
#include <nlohmann/json_fwd.hpp>

namespace met {
  // namespace/typename shorthand inside met::io namespace
  using json = nlohmann::json; 

  namespace io {
    /* json load/save to/from file */
    json load_json(const fs::path &path);
    void save_json(const fs::path &path, const json &js, uint indent = 2);
  }

  /* json (de)serializations for ProjectData type must be declared in met scope*/
  void from_json(const json &js, ProjectData &v);
  void to_json(json &js, const ProjectData &v);

  /* json (de)serializations for ProjectData::Mapp type must be declared in met scope*/
  void from_json(const json &js, ProjectData::CSys &v);
  void to_json(json &js, const ProjectData::CSys &v);

  /* json (de)serialization for BasisTreeNode type must be declared in met scope*/
  void from_json(const json &js, BasisTreeNode &b);
  void to_json(json &js, const BasisTreeNode &b);
} // namespace met

/* json (de)serializations for specific Eigen types must be declared in Eigen scope */
namespace Eigen { 
  void from_json(const met::json& js, met::Spec &v);
  void to_json(met::json &js, const met::Spec &v);
  
  void from_json(const met::json& js, met::Colr &v);
  void to_json(met::json &js, const met::Colr &v);
  
  void from_json(const met::json& js, met::CMFS &v);
  void to_json(met::json &js, const met::CMFS &v);

  void from_json(const met::json &js, met::Basis &b);
  void to_json(met::json &js, const met::Basis &b);
  
  void from_json(const met::json& js, met::Chro &v);
  void to_json(met::json &js, const met::Chro &v);

  // Basis loading
  void from_json(const met::json &js, Array<float, 31, 1> &m);
  void to_json(met::json &js, const Array<float, 31, 1> &m);
  void from_json(const met::json &js, Array<float, 31, 31> &m);
  void to_json(met::json &js, const Array<float, 31, 31> &m);
} // namespace Eigen