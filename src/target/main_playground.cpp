#define EIGEN_MAX_ALIGN_BYTES 32
#define EIGEN_MAX_STATIC_ALIGN_BYTES 32

// STL includes
#include <cstdlib>
#include <exception>
#include <span>
#include <string>
#include <vector>

// Metameric includes
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

const std::string shader_src = R"GLSL(
  #version 460 core

  // Guard functions
  #define guard(expr) if (!(expr)) { return; }

  // Spectrum/color types
  #define Spec  float[wavelength_samples]
  #define CMFS  float[3][wavelength_samples]
  #define Color vec3

  // Wavelength settings
  const uint wavelength_samples = 16;

  // Layout specifiers
  layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;
  layout(binding = 0, std430) restrict readonly buffer  b_0 { Color[] b_0_in;  };
  layout(binding = 1, std430) restrict writeonly buffer b_1 { Color[] b_1_out; };
  layout(location = 0) uniform uint u_n;

  void main() {
    const uint i = gl_WorkGroupID.x * gl_WorkGroupSize.x + gl_LocalInvocationID.x;
    guard(i < u_n);
    b_1_out[i] = b_0_in[i];
  }
)GLSL";

const auto window_flags = gl::WindowCreateFlags::eVisible   | gl::WindowCreateFlags::eDecorated
                        | gl::WindowCreateFlags::eSRGB      | gl::WindowCreateFlags::eFocused
                        | gl::WindowCreateFlags::eResizable | gl::WindowCreateFlags::eDebug;

// Interpret a container type as a span of type T
template <class T, class C>
std::span<T> as_typed_span_aligned(C &c) {
  auto data = c.data();
  guard(data, {});
  return { reinterpret_cast<T*>(data), (c.size() * alignof(typename C::value_type)) / alignof(T) };
}

// Convert a span over type U to 
template <class T, class U>
std::span<T> convert_span_aligned(std::span<U> s) {
  auto data = s.data();
  guard(data, {});
  return { reinterpret_cast<T*>(data), s.size_bytes() / alignof(T) };
}

int main() {
  using namespace met;
  
  using Color   = eig::Array<float, 3, 1>;
  using AlColor = eig::AlArray<float, 3>;

  try {
    gl::Window window = {{ .size  = { 512, 512 }, .title = "Playground", .flags = window_flags }};

    // Prepare test data
    constexpr uint n = 8;
    const Color init_color = 0.f;
    const auto test_v = init_color.reshaped(4, 1).eval();
    std::vector<AlColor> data_i(n, init_color); 
    std::vector<AlColor> data_o(n, init_color); 

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
    std::vector<Color> result(n); 
    std::ranges::transform(data_o, result.begin(), unpadd<>);
    fmt::print("After\n\ti: {}\n\to: {}\n", data_i, result);
  } catch (const std::exception &e) {
    fmt::print(stderr, "{}", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}