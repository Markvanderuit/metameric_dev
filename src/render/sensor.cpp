// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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