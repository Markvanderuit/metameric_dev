#include <metameric/gui/task/mapping_task.hpp>

namespace met {
  const auto gamut_initial_vertices = {
    Color { 0.f, 0.f, 0.f },
    Color { 1.f, 0.f, 5.f },
    Color { 0.f, 1.f, 5.f },
    Color { 1.f, 1.f, 1.f }
  };

  MappingTask::MappingTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void MappingTask::init(detail::TaskInitInfo &info) {
    auto create_flags = gl::BufferCreateFlags::eMapRead | gl::BufferCreateFlags::eMapWrite | 
                        gl::BufferCreateFlags::eMapPersistent | gl::BufferCreateFlags::eMapCoherent;
    auto map_flags = gl::BufferAccessFlags::eMapRead | gl::BufferAccessFlags::eMapWrite | 
                     gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapCoherent;
    auto vertex_byte_span = as_typed_span<std::byte>(gamut_initial_vertices);

    m_gamut_vertex_buffer = {{ .data  = vertex_byte_span, .flags = create_flags }};
    m_gamut_spectra_buffer = {{ .size  = sizeof(Spec) * 4u, .flags = create_flags }};

    m_gamut_vertices = convert_span<Color>(m_gamut_vertex_buffer.map(map_flags ));
    m_gamut_spectra = convert_span<Spec>(m_gamut_spectra_buffer.map(map_flags));
  }

  void MappingTask::eval(detail::TaskEvalInfo &info) {
    // ...
  }
} // namespace met