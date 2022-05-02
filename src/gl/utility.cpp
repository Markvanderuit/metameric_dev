#include <metameric/gl/utility.h>
#include <metameric/gl/vertexarray.h>
#include <metameric/gl/detail/assert.h>
#include <fstream>
#include <span>
#include <fmt/core.h>

namespace metameric::gl {
  std::vector<std::byte> load_shader_binary(fs::path path) {
    // Check that file path exists
    runtime_assert(fs::exists(path),
      fmt::format("load_shader_binary(...), failed to resolve path \"{}\"", path.string()));

    // Attempt to open file stream
    std::ifstream ifs(path, std::ios::ate | std::ios::binary);
    runtime_assert(ifs.is_open(),
      fmt::format("load_shader_binary(...), failed to open file \"{}\"", path.string()));

    // Read file size and construct vector to hold data
    size_t file_size = static_cast<size_t>(ifs.tellg());
    std::vector<std::byte> buffer(file_size);
    
    // Set inpout position to start, then read full file into vector
    ifs.seekg(0);
    ifs.read((char *) buffer.data(), file_size);
    ifs.close();
    
    return buffer;
  }

  gl::Buffer to_indirect(DrawInfo info, BufferStorageFlags flags) {
    runtime_assert(info.array,
      "gl::to_indirect(...), DrawInfo submitted without array object");

    gl::Buffer buffer;
    if (info.array->has_elements()) {
      std::array<uint, 5> data = { info.vertex_count, info.instance_count, 
                                   info.vertex_first, info.vertex_base,
                                   info.instance_base };
      buffer = Buffer({ .size = data.size() * sizeof(uint),
                        .data = std::as_bytes(std::span(data)),
                        .flags = flags });
    } else {
      std::array<uint, 4> data = { info.vertex_count, info.instance_count, 
                                   info.vertex_first, info.instance_base };
      buffer = Buffer({ .size = data.size() * sizeof(uint),
                        .data = std::as_bytes(std::span(data)),
                        .flags = flags });
    }

    return std::move(buffer);
  }

  gl::Buffer to_indirect(ComputeInfo info, BufferStorageFlags flags) {
    std::array<uint, 3> data = { info.groups_x, info.groups_y, info.groups_z };
    gl::Buffer buffer({ .size = data.size() * sizeof(uint),
                        .data = std::as_bytes(std::span(data)),
                        .flags = flags });
    return std::move(buffer);
  }

  namespace state {
    void set(DrawCapability capability, bool enabled) {
      if (enabled) {
        glEnable((uint) capability);
      } else {
        glDisable((uint) capability);
      }
    }

    bool get(DrawCapability capability) {
      return glIsEnabled((uint) capability);
    }

    void set_op(BlendOp src_operand, BlendOp dst_operand) {
      glBlendFunc((uint) src_operand, (uint) dst_operand);
    }

    void set_op(LogicOp operand) {
      glLogicOp((uint) operand);
    }

    void set_viewport(Array2i size, Array2i offset) {
      glViewport(offset[0], offset[1], size[0], size[1]);
    }

    ScopedSet::ScopedSet(DrawCapability capability, bool enabled)
    : _capability(capability), _prev(get(capability)), _curr(enabled) {
      guard(_curr != _prev);
      set(_capability, _curr);
    }

    ScopedSet::~ScopedSet() {
      guard(_curr != _prev);
      set(_capability, _prev);
    }
  } // namespace state
} // namespace metameric::gl