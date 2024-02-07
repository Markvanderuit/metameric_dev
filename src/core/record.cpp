#include <metameric/core/record.hpp>
#include <nlohmann/json.hpp>

namespace met {
  void from_json(const json &js, SurfaceInfo &si) {
    met_trace();
    si.p           = js.at("p").get<eig::Array3f>();
    si.n           = js.at("n").get<eig::Array3f>();
    si.tx          = js.at("tx").get<eig::Array2f>();
    si.diffuse     = js.at("tx").get<Colr>();
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