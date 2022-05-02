#include <metameric/gl/program.h>
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

    GLuint compile_shader_object(const ShaderCreateInfo &info) {
      const GLchar *data_ptr = info.data.data();
      const GLint data_size = info.data.size_bytes();
      
      GLuint handle = glCreateShader((uint) info.type);

      if (info.is_binary_spirv) {
        glShaderBinary(1, &handle, GL_SHADER_BINARY_FORMAT_SPIR_V, data_ptr, data_size);
        glSpecializeShader(handle, info.entry_point.c_str(), 0, nullptr, nullptr);
      } else {
        glShaderSource(handle, 1, &data_ptr, &data_size);
        glCompileShader(handle);
      }
      
      return handle;
    }

    void assert_shader_compilation(GLuint shader) {
      guard(!get_shader_iv(shader, GL_COMPILE_STATUS));

      // Compilation failed, obtain error log
      std::string info(get_shader_iv(shader, GL_INFO_LOG_LENGTH), ' ');
      glGetShaderInfoLog(shader, GLint(info.size()), nullptr, info.data());

      // Format compilation log
      std::stringstream ss_log;
      std::stringstream infss(info);
      for (std::string line; std::getline(infss, line);) {
        guard_continue(line.length() > 2);
        ss_log << fmt::format("{:<8}\n", line);
      }

      // Construct exception with attached compilation log
      metameric::detail::RuntimeException e("Failed to specialize shader");
      e["log"] = ss_log.str();
      throw e;
    }

    void assert_program_linkage(GLuint program) {
      guard(!get_program_iv(program, GL_LINK_STATUS));

      // Compilation failed, obtain error log
      std::string info(get_program_iv(program, GL_INFO_LOG_LENGTH), ' ');
      glGetProgramInfoLog(program, GLint(info.size()), nullptr, info.data());

      // Format compilation log
      std::stringstream ss_log;
      std::stringstream infss(info);
      for (std::string line; std::getline(infss, line);) {
        guard_continue(line.length() > 2);
        ss_log << fmt::format("{:<8}\n", line);
      }

      // Construct exception with attached compilation log
      metameric::detail::RuntimeException e("Failed to link program");
      e["log"] = ss_log.str();
      throw e;
    }
  }
  
  Program::Program(std::initializer_list<ShaderCreateInfo> info)
  : Base(true) {
    guard(_is_init);

    // Generate and compile shader objects
    std::vector<GLuint> shaders;
    std::ranges::transform(info, std::back_inserter(shaders), detail::compile_shader_object);
    std::ranges::for_each(shaders, detail::assert_shader_compilation);

    // Generate and link program object
    _object = glCreateProgram();
    for (const auto &s : shaders) {
      glAttachShader(_object, s);
    }
    glLinkProgram(_object);
    detail::assert_program_linkage(_object);

    // Detach and destroy shader objects
    for (const auto &s : shaders) {
      glDetachShader(_object, s);
      glDeleteShader(s);
    }
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