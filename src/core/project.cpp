#include <metameric/core/project.hpp>
#include <metameric/core/serialization.hpp>
#include <metameric/core/utility.hpp>
#include <fstream>

namespace met {
  namespace detail {
    std::string load_string_from_file(const std::filesystem::path &path) {
      // Check that file path exists
      debug::check_expr(std::filesystem::exists(path),
        fmt::format("failed to resolve path \"{}\"", path.string()));
      
      // Attempt to open file stream
      std::ifstream ifs(path, std::ios::ate);
      debug::check_expr(ifs.is_open(),
        fmt::format("failed to open file \"{}\"", path.string()));
      
      // Read file size and construct string to hold data
      size_t file_size = static_cast<size_t>(ifs.tellg());
      std::string s(file_size, ' ');

      // Set input position to start, then read full file into buffer
      ifs.seekg(0);
      ifs.read((char *) s.data(), file_size);
      ifs.close();

      return s;
    }

    void write_string_to_file(const std::string &s, const std::filesystem::path &path) {
      // Attempt to open output file stream in text mode
      std::ofstream ofs(path, std::ios::out);
      debug::check_expr(ofs.is_open(),
        fmt::format("failed to open file \"{}\"", path.string()));

      // Write string directly to file in text mode
      ofs.write(s.data(), s.size());
      ofs.close();
    }

    json load_json_from_file(const std::filesystem::path &path) {
      return json::parse(load_string_from_file(path));
    }

    void write_json_to_file(const json &j, const std::filesystem::path &path) {
      write_string_to_file(j.dump(2), path);
    }
  } // namespace detail

  namespace io {
    Project load_project_from_file(const std::filesystem::path &path) {
      // Load json representation from file, then deserialize
      json j = detail::load_json_from_file(path);
      return j.get<Project>();
    }

    void write_project_to_file(const Project &p, const std::filesystem::path &path) {
      // Serialize to json representation, then write to file
      json j = p;
      detail::write_json_to_file(j, path);
    }
  } // namespace io
  
  Project::Project(ProjectLoadInfo info) {
    
  }

  Project::Project(ProjectCreateInfo info) {

  }
} // namespace met::io