#pragma once

#include <list>
#include <span>
#include <unordered_set>
#include <vector>
#include <utility>
#include <metameric/core/fwd.hpp>
#include <metameric/core/detail/eigen.hpp>

namespace met {
  template <typename T>
  struct KNNGridCreateInfo {
    // Underlying 3d size of voxel grid
    eig::Array3i grid_size;

    // Boundaries to which positions are clamped
    eig::Array3f space_bounds_min = 0.f;
    eig::Array3f space_bounds_max = 1.f;

    // Optionally provide initialization data fitting at least grid_size
    // std::span<const T> data = { };
  };

  template <typename T>
  class KNNGrid {
  public:
    struct QueryType {
      eig::Array3f position;
      T            value;
      float        distance;
    };
    
    struct ValueType {
      eig::Array3f position;
      T            value;
    };
  
  private:
    std::vector<std::list<ValueType>> 
                 m_grid;
    eig::Array3i m_grid_size;
    eig::Array3f m_space_bounds_min, m_space_bounds_max;

    uint index_from_grid_pos(const eig::Array3i &p) const {
      return p.z() * m_grid_size.y() * m_grid_size.x() + p.y() * m_grid_size.x() + p.x();
    }

    uint nearest_index_from_pos(const eig::Array3f &pos) const {
      auto clamped_pos = pos.min(m_space_bounds_max).max(m_space_bounds_min);
      auto grid_pos = (m_grid_size.cast<float>() * clamped_pos).cast<int>();
      return index_from_grid_pos(grid_pos);
    }

    std::unordered_set<uint> nearest_indices_from_pos(const eig::Array3f &p) const {
      // Obtain maximum and minimum grid coordinates around current position
      auto clamped_pos = p.min(m_space_bounds_max).max(m_space_bounds_min);
      auto grid_pos = (m_grid_size - 1).cast<float>() * clamped_pos;
      auto l = grid_pos.floor().cast<int>().eval(),
           u = grid_pos.ceil().cast<int>().eval();

      // Gather list of all indices possible with these coordinates
      auto indices = { index_from_grid_pos({ l.x(), l.y(), l.z() }),
                       index_from_grid_pos({ l.x(), l.y(), u.z() }),
                       index_from_grid_pos({ l.x(), u.y(), l.z() }),
                       index_from_grid_pos({ l.x(), u.y(), u.z() }),
                       index_from_grid_pos({ u.x(), l.y(), l.z() }),
                       index_from_grid_pos({ u.x(), l.y(), u.z() }),
                       index_from_grid_pos({ u.x(), u.y(), l.z() }),
                       index_from_grid_pos({ u.x(), u.y(), u.z() }) };

      return std::unordered_set(std::make_move_iterator(indices.begin()),
                                std::make_move_iterator(indices.end()));
      // std::unordered_set<uint> set;
      // std::copy_if(indices.begin(), indices.end(), std::inserter(set, set.begin()), [&](uint i) { return !m_grid[i].empty(); });
      // return set;
    }

  public:
    /* constructors */

    KNNGrid() = default;
    KNNGrid(KNNGridCreateInfo<T> info);

    /* insertion functions */

    void insert_1(const T &t, const eig::Array3f &p);
    void insert_n(std::span<const T> t, std::span<eig::Array3f> p);

    /* query functions */

    QueryType              query_1_nearest(const eig::Array3f &p) const;
    std::vector<QueryType> query_k_nearest(const eig::Array3f &p, uint k);
    std::vector<QueryType> query_n_nearest(const eig::Array3f &p);
  };
  
  template <typename T>
  struct VoxelGridCreateInfo {
    // Underlying 3d size of voxel grid
    eig::Array3i grid_size;

    // Boundaries to which positions are clamped
    eig::Array3f space_bounds_min = 0.f;
    eig::Array3f space_bounds_max = 1.f;
  };

  template <typename T>
  class VoxelGrid {
  private:
    std::vector<T> m_grid;
    eig::Array3i   m_grid_size;
    eig::Array3f   m_space_bounds_min, m_space_bounds_max;

  public:
    /* constructors */

    VoxelGrid() = default;
    VoxelGrid(VoxelGridCreateInfo<T> info);

    /* accessors */

    const T& at(const eig::Array3i &p) const {
      return m_grid.at(index_from_grid_pos(p));
    }

    T& at(const eig::Array3i &p) {
      return m_grid.at(index_from_grid_pos(p));
    }

    T query(const eig::Array3f &p) const;
    
    /* direct accessors */

    std::span<const T> data() const { return m_grid; }

    const eig::Array3i& size() const { return m_grid_size; }

    /* helpers */

    eig::Array3i grid_pos_from_pos(const eig::Array3f &p) const {
      auto clamped_p = (p - 0.5f).min(m_space_bounds_max).max(m_space_bounds_min);
      return (m_grid_size.cast<float>() * clamped_p).cast<int>();
    }

    eig::Array3f pos_from_grid_pos(const eig::Array3i &p) const {
      return (p.cast<float>() + 0.5f) / m_grid_size.cast<float>();
    }

    eig::Array3i grid_pos_from_index(uint i) const {
      int w =  m_grid_size.x();
      int wh = m_grid_size.y() * w;
      int mod = i % wh;
      return { mod % w, mod / w, static_cast<int>(i) / wh };
    }  

    uint index_from_grid_pos(const eig::Array3i &p) const {
      return p.z() * m_grid_size.y() * m_grid_size.x() + p.y() * m_grid_size.x() + p.x();
    }
  };
} // namespace met