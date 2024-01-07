#include <metameric/core/utility.hpp>
#include <metameric/render/sensor.hpp>

constexpr static auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
constexpr static auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

namespace met {
  void Sensor::flush() {
    met_trace_full();
    
    if (!m_unif.is_init()) {
      m_unif     = {{ .size = sizeof(UnifLayout), .flags = buffer_create_flags }};
      m_unif_map = m_unif.map_as<UnifLayout>(buffer_access_flags).data();
    }

    m_unif_map->sensor_trf = transform;
    m_unif_map->film_size  = film_size;

    m_unif.flush();
  }
} // namespace met