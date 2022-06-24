// STL includes
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <list>
#include <string>
#include <ranges>
#include <vector>

// Metameric includes
#include <metameric/core/io.hpp>
#include <small_gl/utility.hpp>

// Misc includes
#include <fmt/core.h>

/* 
  Goals:
  - Parse a shader and run through its lines
  - When encountering #include preprocessor statements
    - Parse these statements for available strings
    - Look up an available string or load it from file
    - Replace the available string with its contents
    - Track a traversal queue to prevent recursive includes
  - Output the shader into a temporary
  - Afterwards, parse the shader using GLSLangvalidator
    (this can be done in CMake on the output file though)
 */

auto line_filter(std::string_view s, std::string_view compare) {
  return (s | std::views::split(' ')).front() == compare;
}

auto filter_path_characters(std::string_view s) {
  return s | std::views::filter([](char c) { return !(c == '<' || c == '>' || c == '"'); })
           | std::views::filter([](char c) { return !std::isspace(c); });
}

constexpr auto newl_join  = std::views::transform([](auto &&p) { return p + '\n'; }) 
                          | std::views::join;
constexpr auto incl_filt  = std::views::split(' ') | std::views::drop(1)
                          | std::views::transform(filter_path_characters) | std::views::join;
constexpr auto incl_bind  = std::bind(line_filter, std::placeholders::_1, "#include");

namespace io {
  std::string load_shader_string(const std::filesystem::path &path) {
    // Check that file path exists
    gl::debug::check_expr(std::filesystem::exists(path),
      fmt::format("failed to resolve path \"{}\"", path.string()));

    // Attempt to open file stream
    std::ifstream ifs(path, std::ios::ate | std::ios::binary);
    gl::debug::check_expr(ifs.is_open(),
      fmt::format("failed to open file \"{}\"", path.string()));

    // Read file size and construct string object to hold data
    size_t file_size = static_cast<size_t>(ifs.tellg());
    std::string buffer(file_size, 0);
      
    // Set input position to start, then read full file into buffer
    ifs.seekg(0);
    ifs.read((char *) buffer.data(), file_size);
    ifs.close();

    return buffer;
  }
} // namespace io

std::string load_shader_impl(const std::filesystem::path      &path, 
                             std::list<std::filesystem::path> &prev, 
                             std::list<std::filesystem::path> &stack) {
  // Assert that path is not currently in traversal stack
  gl::debug::check_expr(std::ranges::find(stack, path) == stack.end(),
    fmt::format("Potentially recursive include detected in shader \"{}\"", path.string()));
  stack.push_back(path);
  
  // Prevent unnecessary double includes as performance measure
  guard(std::ranges::find(prev, path) == prev.end(), "");
  prev.push_back(path);
  
  // Load shader file into string
  std::string shader_string = io::load_shader_string(path);

  // Obtain a vector over the string's separated lines
  auto split_view = shader_string | std::views::split('\n');
  std::vector<std::string> shader_vector(split_view.begin(), split_view.end());

  // Operate on and modify line-split strings starting with #include
  std::ranges::for_each(shader_vector | std::views::filter(incl_bind), [&](std::string &s) {
    // Obtain a filtered path
    std::string include_path;
    std::ranges::copy(s | incl_filt, std::back_inserter(include_path));

    // Recursively load and immediately insert a shader
    s = load_shader_impl(include_path, prev, stack);
  });

  // Rejoin the line-split vector into a single string
  shader_string.clear();
  std::ranges::copy(shader_vector | newl_join, std::back_inserter(shader_string));

  stack.pop_back();
  return shader_string;
}

std::string load_shader(std::filesystem::path path) {
  std::list<std::filesystem::path> prev;
  std::list<std::filesystem::path> stack;

  return load_shader_impl(path, prev, stack);
}

void run() {
  std::string s = load_shader("resources/shaders/viewport_task/gamut_draw.frag");
  fmt::print("{}", s);
}

int main() {
  try {
    run();
  } catch (const std::exception &e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}