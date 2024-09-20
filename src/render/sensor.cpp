#include <metameric/core/utility.hpp>
#include <metameric/render/sensor.hpp>

namespace met {
  void Sensor::flush() {
    met_trace_full();
    
    if (!m_unif.is_init())
      std::tie(m_unif, m_unif_map) = gl::Buffer::make_flusheable_object<UnifLayout>();

    m_unif_map->full_trf  = proj_trf * view_trf;
    m_unif_map->proj_trf  = proj_trf;
    m_unif_map->view_trf  = view_trf;
    m_unif_map->film_size = film_size;
    
    m_unif.flush();
  }

  void PixelSensor::flush() {
    met_trace_full();
    
    if (!m_unif.is_init())
      std::tie(m_unif, m_unif_map) = gl::Buffer::make_flusheable_object<UnifLayout>();

    m_unif_map->full_trf  = proj_trf * view_trf;
    m_unif_map->proj_trf  = proj_trf;
    m_unif_map->view_trf  = view_trf;
    m_unif_map->film_size = film_size;
    m_unif_map->pixel     = pixel;
    
    m_unif.flush();
  }

  void RaySensor::flush() {
    met_trace_full();
    
    if (!m_unif.is_init())
      std::tie(m_unif, m_unif_map) = gl::Buffer::make_flusheable_object<UnifLayout>();

    m_unif_map->origin    = origin;
    m_unif_map->direction = direction;

    m_unif.flush();
  }
} // namespace met