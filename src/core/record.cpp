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

#include <metameric/core/record.hpp>
#include <nlohmann/json.hpp>

namespace met {
  void from_json(const json &js, SurfaceInfo &si) {
    met_trace();
    si.p           = js.at("p").get<eig::Array3f>();
    si.n           = js.at("n").get<eig::Array3f>();
    si.tx          = js.at("tx").get<eig::Array2f>();
    si.diffuse     = js.at("diffuse").get<Colr>();
    si.record.data = js.at("record").get<uint>();
  }

  void to_json(json &js, const SurfaceInfo &si) {
    met_trace();
    js = {{ "p",       si.p           },
          { "n",       si.n           },
          { "tx",      si.tx          },
          { "diffuse", si.diffuse     },
          { "record",  si.record.data }};
  }
} // namespace met