// #define EIGEN_MAX_STATIC_ALIGN_BYTES 0

// Metameric includes
#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>

// GL includes
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <small_gl/window.hpp>
#include <small_gl_parser/parser.hpp>

// STL includes
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <numeric>
#include <span>
#include <string>
#include <vector>


const std::string shader_src = R"GLSL(
  #version 460 core

  // Guard functions
  #define guard(expr) if (!(expr)) { return; }

  // Wavelength settings
  const uint data_samples = DATA_WIDTH;

  // Common types
  #define SpecType float[data_samples]
  #define CMFSType float[3][data_samples]

  struct DataObject {
    CMFSType c;
    SpecType t;
    float i;
    // float v2[3]; /*  padding */
  };

  // Layout specifiers
  layout(local_size_x = 256) in;
  layout(std430)             buffer;

  layout(binding = 0) restrict readonly  buffer b_0 { DataObject data[]; } b_in;
  layout(binding = 1) restrict writeonly buffer b_1 { DataObject data[]; } b_out;

  layout(location = 0) uniform uint u_n;

  void main() {
    const uint i = gl_GlobalInvocationID.x;
    guard(i < u_n);

    DataObject o = b_in.data[i];

    for (uint j = 0; j < data_samples; ++j) {
      b_out.data[i].t[j] = o.t[j];  
    }
    b_out.data[i].c = o.c;
    // b_out.data[i].t = o.t;
    b_out.data[i].i = o.i;
  }
)GLSL";

int main() {
  using namespace met;
  
  constexpr uint DataRows = 32;

  using SpecType     = eig::Array<float, DataRows, 1>;
  using CMFSType     = eig::Array<float, DataRows, 3>;
  using CMFSTypeUnAl = eig::Array<float, DataRows, 3, eig::DontAlign>;
  using SpecTypeUnAl = eig::Array<float, DataRows, 1, eig::DontAlign>;

  struct DataObject {
    CMFSTypeUnAl c;
    SpecTypeUnAl t;
    float i;
    // float v2[3];
  };

  try {
    // Set up OpenGL
    gl::Window window = {{ .size = { 1, 1}, .flags = gl::WindowCreateFlags::eDebug }};
    gl::debug::enable_messages(gl::DebugMessageSeverity::eLow, gl::DebugMessageTypeFlags::eAll);

    // Prepare input data
    DataObject data_v;
    std::iota(range_iter(data_v.t), 0.f);
    std::iota(range_iter(data_v.t), 16.f);
    data_v.i = 314.f;
    std::vector<DataObject> data(8, data_v);

    // Prepare buffer objects
    gl::Buffer buffer_in  = {{ .data = as_span<const std::byte>(data)              }};
    gl::Buffer buffer_out = {{ .size = as_span<const std::byte>(data).size_bytes() }};

    // Prepare shader parser
    glp::Parser parser;
    parser.add_string("DATA_WIDTH", std::to_string(DataRows));

    // Prepare compute shader
    gl::Program program = {{ .type   = gl::ShaderType::eCompute, 
                             .data   = as_span<const std::byte>(shader_src),
                             .parser = &parser }};
    program.uniform("u_n", static_cast<uint>(data.size()));
    gl::ComputeInfo dispatch = { .groups_x = ceil_div(static_cast<uint>(data.size()), 256u), .bindable_program = &program };

    // Dispatch shader
    buffer_in.bind_to(gl::BufferTargetType::eShaderStorage,  0);
    buffer_out.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    gl::dispatch_compute(dispatch);

    // Copy back and read data
    std::vector<DataObject> result(data.size());
    buffer_out.get(as_span<std::byte>(result));
    
    
    for (uint i = 0; i < data.size(); ++i) {
      fmt::print("{}, {}, t = {}, i = {}\n", i, "a", data[i].t, data[i].i);
      fmt::print("{}, {}, t = {}, i = {}\n", i, "b", result[i].t, result[i].i);
    }
    // fmt::print("in    : {}\n", data);
    // fmt::print("out   : {}\n", result);


    constexpr auto data_eq = [](const DataObject &a, const DataObject &b) { 
      return (a.c == b.c).all() &&
             (a.t == b.t).all() && 
              a.i == b.i; };
    fmt::print("equal : {}\n", std::equal(range_iter(data), range_iter(result), data_eq));
    fmt::print("bytes : {}\n", sizeof(DataObject));
    
    /* // Prepare test data
    constexpr uint n = 8;
    const Colr init_color = 0.f;
    const auto test_v = init_color.reshaped(4, 1).eval();
    std::vector<AlColr> data_i(n, init_color); 
    std::vector<AlColr> data_o(n, init_color); 

    for (uint i = 0; i < n; ++i) { data_i[i] = float(i); }
    
    // Prepare buffer objects
    gl::Buffer buffer_i = {{ .data = as_span<const std::byte>(data_i) }};
    gl::Buffer buffer_o = {{ .data = as_span<const std::byte>(data_o) }};

    // Prepare compute shader
    gl::Program program = {{ .type = gl::ShaderType::eCompute, .data = as_span<const std::byte>(shader_src) }};
    program.uniform<uint>("u_n", n);
    gl::ComputeInfo compute_info = { .groups_x = ceil_div(n, 256), .bindable_program = &program };

    // Perform compute call
    buffer_i.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    buffer_o.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    gl::dispatch_compute(compute_info);

    // Copy back and read data
    fmt::print("Before\n\ti: {}\n\to: {}\n", data_i, data_o);
    buffer_o.get(as_span<std::byte>(data_o));
    std::vector<Colr> result(n); 
    std::ranges::transform(data_o, result.begin(), unpadd<>);
    fmt::print("After\n\ti: {}\n\to: {}\n", data_i, result); */
  } catch (const std::exception &e) {
    fmt::print(stderr, "{}", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}