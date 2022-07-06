#include <metameric/gui/spectral_grid.hpp>
#include <metameric/core/utility.hpp>
#include <algorithm>
#include <execution>
#include <mutex>

namespace met {
  namespace detail {
    float sq_euql_dist(const eig::Array3f &a, const eig::Array3f &b) {
      return (b - a).pow(2).sum();
    }
    
    float eucl_dist(const eig::Array3f &a, const eig::Array3f &b) {
      return std::sqrtf(sq_euql_dist(a, b));
    }
  } // namespace detail

  template <typename T>
  KNNGrid<T>::KNNGrid(VoxelGridCreateInfo<T> info)
  : m_grid(info.grid_size.prod()),
    m_grid_size(info.grid_size),
    m_space_bounds_min(info.space_bounds_min),
    m_space_bounds_max(info.space_bounds_max),
    m_value_bounds_min(info.value_bounds_min),
    m_value_bounds_max(info.value_bounds_max) {

    // Insert clamped data into grid if provided
    // if (std::span<T> data = info.data; data.size()) {
    //   debug::check_expr(data.size() == m_grid.size(),
    //     "grid size does not match provided data size");
    //   std::transform(std::execution::par_unseq, data.begin(), data.end(), m_grid.begin(),
    //     [info](const auto &t) { return std::clamp(t, info.value_bounds_min, info.value_bounds_max); });
    // }
  }

  template <typename T>
  void KNNGrid<T>::insert_multiple(std::span<const T> t, std::span<eig::Array3f> positions) {
    std::vector<std::mutex> lock_grid(m_grid_size.prod());
    std::vector<uint> indices(positions.size());
    
    std::for_each(std::execution::par, indices.begin(), indices.end(), [&](uint i) {
      auto &pos = positions[i];
      uint j = index_from_pos(pos);

      // Obtain a lock on voxel position and push data on to list
      std::lock_guard lock(lock_grid[j]);
      m_grid[j].push_back({ pos, t[i] });
    });
  }

  template <typename T>
  void KNNGrid<T>::insert_single(const T &t, const eig::Array3f &pos) {
    uint i = index_from_pos(pos);
    m_grid[i].push_back({ pos, t });
  }

  template <typename T>
  T KNNGrid<T>::query_nearest(const eig::Array3f &pos) const {
    float min_dist  = FLT_MAX;
    T     min_value = 0.f;
    
    for (uint i : nearest_indices_from_pos(pos)) {
      auto &voxel = m_grid[i];
      guard_continue(!voxel.empty());

      for (const ValueType &pair : voxel) {
        auto &[new_pos, new_value] = pair;
        float new_dist = detail::eucl_dist(pos, new_pos);
        if (new_dist < min_dist) {
          min_dist  = new_dist;
          min_value = new_value;
        }
      }
    }

    return min_value;
  }

  /* Explicit template instantiations of met::KNNGrid<T> */

  template class KNNGrid<float>;
} // namespace met