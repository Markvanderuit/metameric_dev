#include <metameric/gl/program.h>
#include <metameric/gl/utility.h>
#include <metameric/gl/detail/assert.h>
#include <ranges>

namespace metameric::gl {
  namespace detail {
    GLint get_shader_iv(GLuint object, GLenum name) {
      GLint value;
      glGetShaderiv(object, name, &value);
      return value;
    }

    GLint get_program_iv(GLuint object, GLenum name) {
      GLint value;
      glGetProgramiv(object, name, &value);
      return value;
    }

    std::string fmt_info_log(const std::string &log) {
      std::stringstream ss_o, ss_i(log);
      for (std::string line; std::getline(ss_i, line);) {
        guard_continue(line.length() > 2);
        ss_o << fmt::format("{:<8}\n", line);
      }
      return ss_o.str();
    }

    void assert_shader_compile(GLuint shader) {
      guard(!get_shader_iv(shader, GL_COMPILE_STATUS));

      // Compilation failed, obtain error log
      std::string info(get_shader_iv(shader, GL_INFO_LOG_LENGTH), ' ');
      glGetShaderInfoLog(shader, GLint(info.size()), nullptr, info.data());

      // Construct exception with attached compilation log
      metameric::detail::RuntimeException e("Failed to specialize shader");
      e["log"] = fmt_info_log(info);
      throw e;
    }

    void assert_program_link(GLuint program) {
      guard(!get_program_iv(program, GL_LINK_STATUS));

      // Compilation failed, obtain error log
      std::string info(get_program_iv(program, GL_INFO_LOG_LENGTH), ' ');
      glGetProgramInfoLog(program, GLint(info.size()), nullptr, info.data());

      // Construct exception with attached compilation log
      metameric::detail::RuntimeException e("Failed to link program");
      e["log"] = fmt_info_log(info);
      throw e;
    }

    GLuint attach_shader_object(GLuint program, const ShaderCreateInfo &info) {
      GLuint handle = glCreateShader((uint) info.type);
      const auto [ptr, size] = std::tuple { (const GLchar *) info.data.data(), (GLint) info.data.size_bytes() };
      
      // Compile or specialize shader and check for correctness
      if (info.is_binary_spirv) {
        glShaderBinary(1, &handle, GL_SHADER_BINARY_FORMAT_SPIR_V, ptr, size);
        glSpecializeShader(handle, info.entry_point.c_str(), 0, nullptr, nullptr);
      } else {
        glShaderSource(handle, 1, &ptr, &size);
        glCompileShader(handle);
      }
      assert_shader_compile(handle);

      glAttachShader(program, handle);
      return handle;
    }

    void detach_shader_object(GLuint program, GLuint shader) {
      glDetachShader(program, shader);
      glDeleteShader(shader);
    }

    GLuint constr_program_object(std::vector<ShaderCreateInfo> info) {
      GLuint handle = glCreateProgram();
      
      std::vector<GLuint> shaders;
      shaders.reserve(info.size());

      // Generate, compile, and attach shader objects
      std::ranges::transform(info, std::back_inserter(shaders),
        [handle] (const auto &i) { return attach_shader_object(handle, i); });

      // Link program and check for correctness
      glLinkProgram(handle);
      detail::assert_program_link(handle);

      // Detach and destroy shader objects
      std::ranges::for_each(shaders, 
        [handle] (const auto &i) { detach_shader_object(handle, i); });
      
      return handle;
    }
  }

  Program::Program(std::initializer_list<ShaderLoadInfo> info)
  : Base(true) {
    guard(_is_init);

    // Load binary shader data
    std::vector<std::vector<std::byte>> shader_bins;
    shader_bins.reserve(info.size());
    std::ranges::transform(info, std::back_inserter(shader_bins),
      [](const auto &i) { return load_shader_binary(i.file_path); });

    // Construct shader create info
    std::vector<ShaderCreateInfo> shader_info;
    std::transform(shader_bins.begin(), shader_bins.end(), 
                   info.begin(), std::back_inserter(shader_info),
      [&] (const auto &shader, const auto& i) {
      return ShaderCreateInfo { i.type, shader, i.is_binary_spirv, i.entry_point };
    });
    
    _object = detail::constr_program_object(shader_info);
  }
  
  Program::Program(std::initializer_list<ShaderCreateInfo> shader_info)
  : Base(true) {
    guard(_is_init);
    _object = detail::constr_program_object(shader_info);
  }

  Program::~Program() {
    guard(_is_init);
    glDeleteProgram(_object);
  }

  void Program::bind() const {
    glUseProgram(_object);
  }

  void Program::unbind() const {
    glUseProgram(0);
  }

  int Program::loc(std::string_view s) {
    auto f = _loc.find(s.data());
    if (f == _loc.end()) {
      GLint handle = glGetUniformLocation(_object, s.data());
      runtime_assert(handle >= 0, 
        fmt::format("Program::location(...), failed for string \"{}\"", s));
      f = _loc.insert({s.data(), handle}).first;
    }
    return f->second;
  }
    
  #define MET_IMPL_UNIFORM(type, type_short)\
    template <> void Program::uniform<type>\
    (std::string_view s, type v)\
    { glProgramUniform1 ## type_short (_object, loc(s), v); }\
    template <> void Program::uniform<eig::Array<type, 2, 1>>\
    (std::string_view s, eig::Array<type, 2, 1> v)\
    { glProgramUniform2 ## type_short (_object, loc(s), v[0], v[1]); }\
    template <> void Program::uniform<eig::Array<type, 3, 1>>\
    (std::string_view s, eig::Array<type, 3, 1> v)\
    { glProgramUniform3 ## type_short (_object, loc(s), v[0], v[1], v[2]); }\
    template <> void Program::uniform<eig::Array<type, 4, 1>>\
    (std::string_view s, eig::Array<type, 4, 1> v)\
    { glProgramUniform4 ## type_short (_object, loc(s), v[0], v[1], v[2], v[3]); }\
    template <> void Program::uniform<eig::Vector<type, 2>>\
    (std::string_view s, eig::Vector<type, 2> v)\
    { glProgramUniform2 ## type_short (_object, loc(s), v[0], v[1]); }\
    template <> void Program::uniform<eig::Vector<type, 3>>\
    (std::string_view s, eig::Vector<type, 3> v)\
    { glProgramUniform3 ## type_short (_object, loc(s), v[0], v[1], v[2]); }\
    template <> void Program::uniform<eig::Vector<type, 4>>\
    (std::string_view s, eig::Vector<type, 4> v)\
    { glProgramUniform4 ## type_short (_object, loc(s), v[0], v[1], v[2], v[3]); }

  #define MET_IMPL_UNIFORM_MAT(type, type_short)\
    template <> void Program::uniform<eig::Array<type, 2, 2>>\
    (std::string_view s, eig::Array<type, 2, 2> v)\
    { glProgramUniformMatrix2 ## type_short ## v(_object, loc(s), 1, false, v.data()); }\
    template <> void Program::uniform<eig::Array<type, 3, 3>>\
    (std::string_view s, eig::Array<type, 3, 3> v)\
    { glProgramUniformMatrix4 ## type_short ## v(_object, loc(s), 1, false, v.data()); }\
    template <> void Program::uniform<eig::Array<type, 4, 4>>\
    (std::string_view s, eig::Array<type, 4, 4> v)\
    { glProgramUniformMatrix4 ## type_short ## v(_object, loc(s), 1, false, v.data()); }\
    template <> void Program::uniform<eig::Matrix<type, 2, 2>>\
    (std::string_view s, eig::Matrix<type, 2, 2> v)\
    { glProgramUniformMatrix2 ## type_short ## v(_object, loc(s), 1, false, v.data()); }\
    template <> void Program::uniform<eig::Matrix<type, 3, 3>>\
    (std::string_view s, eig::Matrix<type, 3, 3> v)\
    { glProgramUniformMatrix4 ## type_short ## v(_object, loc(s), 1, false, v.data()); }\
    template <> void Program::uniform<eig::Matrix<type, 4, 4>>\
    (std::string_view s, eig::Matrix<type, 4, 4> v)\
    { glProgramUniformMatrix4 ## type_short ## v(_object, loc(s), 1, false, v.data()); }

  // Explicit template specializations
  MET_IMPL_UNIFORM(bool, ui)
  MET_IMPL_UNIFORM(uint, ui)
  MET_IMPL_UNIFORM(int, i)
  MET_IMPL_UNIFORM(float, f)
  MET_IMPL_UNIFORM_MAT(float, f)
} // namespace metameric::gl