#pragma once

#include <list>
#include <span>
#include <unordered_set>
#include <vector>
#include <utility>
#include <metameric/core/fwd.hpp>
#include <metameric/core/detail/eigen.hpp>

namespace met {
  enum class VoxelGridQueryType {
    eNearest,
    eLinear,
    eNormal
  }; 

  template <typename T>
  struct VoxelGridCreateInfo {
    // Underlying 3d size of voxel grid
    eig::Array3i grid_size;

    // Boundaries to which positions are clamped
    eig::Array3f space_bounds_min = 0.f;
    eig::Array3f space_bounds_max = 1.f;

    // Boundaries to which values are clamped
    T value_bounds_min = 0.f;
    T value_bounds_max = 1.f;

    // Optionally provide initialization data fitting at least grid_size
    // std::span<const T> data = { };
  };

  template <typename T>
  class KNNGrid {
    using ValueType = std::pair<eig::Array3f, T>;

    std::vector<std::list<ValueType>> 
                 m_grid;
    eig::Array3i m_grid_size;
    eig::Array3f m_space_bounds_min, m_space_bounds_max;
    T            m_value_bounds_min, m_value_bounds_max;

    uint index_from_grid_pos(const eig::Array3i &grid_pos) const {
      return grid_pos.z() * m_grid_size.y() * m_grid_size.x()
           + grid_pos.y() * m_grid_size.x()
           + grid_pos.x();
    }

    uint index_from_pos(const eig::Array3f &pos) const {
      auto clamped_pos = pos.min(m_space_bounds_max).max(m_space_bounds_min);
      auto grid_pos = (m_grid_size.cast<float>() * clamped_pos).cast<int>();
      return index_from_grid_pos(grid_pos);
    }

    std::unordered_set<uint> nearest_indices_from_pos(const eig::Array3f &pos) const {
      // Obtain maximum and minimum grid coordinates around current position
      auto clamped_pos = pos.min(m_space_bounds_max).max(m_space_bounds_min);
      auto grid_pos = m_grid_size.cast<float>() * clamped_pos;
      auto l = grid_pos.floor().cast<int>().eval(),
           u = grid_pos.ceil().cast<int>().eval();

      // Gather list of all indices possible with these coordinates
      auto indices = { index_from_grid_pos({ l.x(), l.y(), l.z() }), index_from_grid_pos({ l.x(), l.y(), u.z() }),
                       index_from_grid_pos({ l.x(), u.y(), l.z() }), index_from_grid_pos({ l.x(), u.y(), u.z() }),
                       index_from_grid_pos({ u.x(), l.y(), l.z() }), index_from_grid_pos({ u.x(), l.y(), u.z() }),
                       index_from_grid_pos({ u.x(), u.y(), l.z() }), index_from_grid_pos({ u.x(), u.y(), u.z() }) };

      //  Copy over only indices for voxels that are non-empty
      std::unordered_set<uint> set;
      std::copy_if(indices.begin(), indices.end(), std::inserter(set, set.begin()), [&](uint i) { return !m_grid[i].empty(); });
      return set;
    }

  public:
    /* constructors */

    KNNGrid() = default;
    KNNGrid(VoxelGridCreateInfo<T> info);

    /* insertion/query functions */

    void insert_multiple(std::span<const T> t, std::span<eig::Array3f> positions);
    void insert_single(const T &t, const eig::Array3f &pos);
    T query_nearest(const eig::Array3f &pos) const;
  };
} // namespace met