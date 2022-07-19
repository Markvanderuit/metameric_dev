#include <metameric/core/serialization.hpp>
#include <metameric/core/utility.hpp>
#include <nlohmann/json.hpp>
#include <string.h>

// Shorthand of json type

namespace met {
  struct Object {
    std::string name;
    int         value;
    Spec        spectrum;
  };

  void from_json(const json &stream, Object &p) {
    p.name     = stream.at("name").get<std::string>();
    p.value    = stream.at("value").get<int>();
    p.spectrum = stream.at("spectrum").get<Spec>();
  }
  
  void to_json(json &stream, const Object &p) {
    stream["name"]     = p.name;
    stream["value"]    = p.value;
    stream["spectrum"] = p.spectrum;
  }
} // namespace met

int main() {
  using namespace met;
  
  // Construct simple object
  met::Object object = { 
    .name     = "John", 
    .value    = 5, 
    .spectrum = models::emitter_cie_d65 
  };

  // Output object data
  fmt::print("object\n\tname: {}\n\tvalue: {}\n\tspectrum: {}\n",
    object.name, object.value, object.spectrum);

  // Serialize and output object
  met::json j = object;
  fmt::print("{}\n", j.dump(2));

  object.name     = "blargh!";
  object.spectrum = 1.f;

  // object = j.get<Object>();
  // Output object data
  fmt::print("object\n\tname: {}\n\tvalue: {}\n\tspectrum: {}\n",
    object.name, object.value, object.spectrum);

  return 0;
}