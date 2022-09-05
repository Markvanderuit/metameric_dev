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
#include <fmt/core.h>

// glslang includes
// #include <glslang/SPIRV/
// #include <glslang/Include/
// #include <shaderc/shaderc.hpp>
#include <spirv_glsl.hpp>

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

  #define guard(expr) if (!(expr)) { return; }

  layout(local_size_x = 256) in;
  layout(std430)             buffer;

  layout(binding = 0) restrict readonly  buffer b_0 { float data[]; } b_in;
  layout(binding = 1) restrict writeonly buffer b_1 { float data[]; } b_out;

  layout(location = 0) uniform uint  u_n;
  layout(location = 1) uniform float u_mult;

  void main() {
    const uint i = gl_GlobalInvocationID.x;
    guard(i < u_n);

    b_out.data[i] = u_mult * b_in.data[i];
  }
)GLSL";

GLint get_prg_interface_iv(gl::Program &program, GLenum interface, GLenum pname) {
  GLint param;
  glGetProgramInterfaceiv(program.object(), interface, pname, &param);
  return param;
}

GLint get_prg_resource_iv(gl::Program &program, GLenum interface, GLuint index, GLenum property) {
  GLint param;
  glGetProgramResourceiv(program.object(), interface, index, 1, &property, 1, nullptr, &param);
  return param;
}

GLint get_prg_resource_idx(gl::Program &program, GLenum interface, const std::string &name) {
  return 0;
}

std::string get_prg_resource_name(gl::Program &program, GLenum interface, GLuint index) {
  std::string buffer(' ', get_prg_interface_iv(program, interface, GL_MAX_NAME_LENGTH));
  fmt::print("{}\n", buffer.size());
  GLsizei len; 
  glGetProgramResourceName(program.object(), interface, index, buffer.size(), &len, buffer.data());
  buffer.resize(len);
  return buffer;
}

std::vector<GLint> get_prg_resource_vars(gl::Program &program, GLenum interface, GLuint index) {
  auto n_vars = get_prg_resource_iv(program, GL_UNIFORM_BLOCK, index, GL_NUM_ACTIVE_VARIABLES);
  GLenum prop = GL_ACTIVE_VARIABLES;

  std::vector<GLint> vars(n_vars);
  glGetProgramResourceiv(program.object(), interface, index, 1, &prop, vars.size(), nullptr, vars.data());

  return vars;
}

/* std::vector<std::byte> compile_glsl_to_spirv(const std::vector<std::byte> &glsl) {
  shaderc::CompileOptions options;
  options.SetSourceLanguage(shaderc_source_language_glsl);
  options.SetOptimizationLevel(shaderc_optimization_level_performance);
  options.SetTargetEnvironment(shaderc_target_env_opengl, shaderc_env_version_opengl_4_5);
  options.SetTargetSpirv(shaderc_spirv_version_1_5);

  auto result = shaderc::Compiler().CompileGlslToSpvAssembly(
    (const char *) glsl.data(), shaderc_compute_shader, nullptr, options);

  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    fmt::print(stderr, "compile_glsl_to_spirv(...) output: {}\n", result.GetErrorMessage());
    return { };
  }

  return std::vector((const std::byte *) result.cbegin(), (const std::byte *) result.cend());
} */

int main() {
  using namespace met;
  namespace spvc = spirv_cross;
  
  // try {
    // Set up OpenGL
    gl::Window window = {{ .size = { 1, 1}, .flags = gl::WindowCreateFlags::eDebug }};
    gl::debug::enable_messages(gl::DebugMessageSeverity::eLow, gl::DebugMessageTypeFlags::eAll);

    // Load shader binary
    std::vector<std::byte>     spv_b = gl::io::load_shader_binary("resources/shaders/gen_color_mappings/gen_color_mapping_cl.comp.spv.opt");
    std::vector<std::uint32_t> spv_i(range_iter(cnt_span<std::uint32_t>(spv_b)));
    // std::vector<std::byte>     spv_b = gl::io::load_shader_binary("resources/shaders/misc/playground.comp.spv");

    spvc::CompilerGLSL compiler(std::move(spv_i));

    // Query reflection information
    spvc::ShaderResources resources = compiler.get_shader_resources();

    auto vars = compiler.get_active_interface_variables();
    
    compiler.set_common_options({ 
      .vulkan_semantics = true
    });
    auto str = compiler.compile();
    fmt::print("{}\n", str);

    for (auto &resource : resources.storage_buffers) {
      auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
      auto location = compiler.get_decoration(resource.id, spv::DecorationLocation);
      auto name2 = compiler.get_name(resource.id);
      fmt::print("ssbo resource {}, binding {}, location {}, fname {}\n", resource.name, binding, location, name2);

    }

    for (auto &resource : resources.uniform_buffers) {
      auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
      auto location = compiler.get_decoration(resource.id, spv::DecorationLocation);
      fmt::print("ubo resource {}, binding {}, location {}\n", resource.name, binding, location);
    }

    for (auto &resource : resources.push_constant_buffers) {
      auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
      auto location = compiler.get_decoration(resource.id, spv::DecorationLocation);
      fmt::print("pc resource {}, binding {}, location {}\n", resource.name, binding, location);
    }

    for (auto &resource : resources.stage_inputs) {
      auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
      auto location = compiler.get_decoration(resource.id, spv::DecorationLocation);
      fmt::print("inp resource {}, binding {}, location {}\n", resource.name, binding, location);
    }

    for (auto &resource : resources.subpass_inputs) {
      auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
      auto location = compiler.get_decoration(resource.id, spv::DecorationLocation);
      fmt::print("sp inp resource {}, binding {}, location {}\n", resource.name, binding, location);
    }

    // for (auto &active : compiler.get_blo) {

    // }

    // Complete program execution
    fmt::print("Bye!\n");
    std::exit(0);

    // Prepare input data
    constexpr uint n = 1024;
    std::vector<float> data(n);
    std::iota(range_iter(data), 1.f);

    // Prepare buffer objects
    gl::Buffer buffer_in  = {{ .data = cnt_span<const std::byte>(data)              }};
    gl::Buffer buffer_out = {{ .size = cnt_span<const std::byte>(data).size_bytes() }};

    // Prepare uniform buffer
    struct UniformData {
      uint  n;
      float mult;
    };
    UniformData uniform_data { .n = n, .mult = 2.f };
    std::span<const std::byte> uniform_span((const std::byte *) &uniform_data,
                                            sizeof(decltype(uniform_data)));
    gl::Buffer buffer_un = {{ .data = uniform_span }};

    // Prepare compute shader
    // gl::Program program = {{ .type   = gl::ShaderType::eCompute, 
    //                          .path   = "resources/shaders/misc/playground.comp" }};
    gl::Program program = {{ 
      .type   = gl::ShaderType::eCompute, 
      .path   = "resources/shaders/misc/playground.comp.spv",
      .is_spirv_binary = true 
    }};
    auto obj = program.object();



    auto n_unif_blocks = get_prg_interface_iv(program, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES);
    fmt::print("Blocks: {}\n", n_unif_blocks);
    for (uint i = 0; i < n_unif_blocks; ++i) {
      fmt::print("Block {}", i);

      auto vars = get_prg_resource_vars(program, GL_UNIFORM_BLOCK, i);
      if (!vars.size()) continue;

      for (uint j = 0; j < vars.size(); ++j) {
        auto loc  = get_prg_resource_iv(program, GL_UNIFORM, vars[j], GL_LOCATION);
        auto name = get_prg_resource_name(program, GL_UNIFORM, vars[j]);

        fmt::print("Block {}, var {}, loc={}, name={}\n", i, j, loc, name);
        // fmt::print("Resource is ubo {}, name: {}\n", is_ubo, resource_name); 
      }
    }

    // auto unif_loc_0 = glGetUniformLocation(program.object(), "u_0.mult");
    // auto unif_loc_1 = glGetUniformLocation(program.object(), "u_in.mult");
    // fmt::print("loc0={}, loc1={}\n", unif_loc_0, unif_loc_1);

    fmt::print("Bye!\n");
    std::exit(0);
    // Set uniforms directly, instead of relying on the program object to provide binding information
    // glProgramUniform1ui(program.object(), 0, static_cast<uint>(data.size()));
    // glProgramUniform1f(program.object(), 1, 2.f);
    
    // auto u_n = glGetUniformLocation(program.object(), "u_n");
    // fmt::print("u_n {}\n", u_n);
    // std::exit(0);


    gl::ComputeInfo dispatch = { .groups_x = ceil_div(static_cast<uint>(data.size()), 256u), 
                                 .bindable_program = &program };

    // Dispatch shader
    buffer_in.bind_to(gl::BufferTargetType::eShaderStorage,  0);
    buffer_out.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    buffer_un.bind_to(gl::BufferTargetType::eUniform, 0);
    gl::dispatch_compute(dispatch);

    // Copy back and print data
    std::vector<float> result(data.size());
    buffer_out.get(cnt_span<std::byte>(result));
    for (uint i = 0; i < data.size(); ++i) {
      fmt::print("{} -> {}\n", data[i], result[i]);
    }
  // } catch (const std::exception &e) {
  //   fmt::print(stderr, "{}", e.what());
  //   return EXIT_FAILURE;
  // }
  return EXIT_SUCCESS;
}