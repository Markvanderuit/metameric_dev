#pragma once

#include <list>
#include <span>
#include <unordered_set>
#include <vector>
#include <metameric/core/math.hpp>

namespace met {
  struct GridCreateInfo {
    // Underlying 3d size of voxel grid
    eig::Array3i grid_size;

    // Boundaries to which positions are clamped
    eig::Array3f space_bounds_min = 0.f;
    eig::Array3f space_bounds_max = 1.f;
  };

  class AbstractGrid {
  protected:
    eig::Array3i m_grid_size;
    eig::Array3f m_space_bounds_min, m_space_bounds_max;

    AbstractGrid(GridCreateInfo info);

  public:
    uint index_from_grid_pos(const eig::Array3i &p) const {
      return p.z() * m_grid_size.y() * m_grid_size.x() 
           + p.y() * m_grid_size.x() + p.x();
    }

    eig::Array3i grid_pos_from_index(uint i) const {
      int w =  m_grid_size.x();
      int wh = m_grid_size.y() * w;
      int mod = i % wh;
      return { mod % w, mod / w, static_cast<int>(i) / wh };
    }

    eig::Array3f pos_from_grid_pos(const eig::Array3i &p) const {
      return (p.cast<float>() + 0.5f) / (m_grid_size).cast<float>();
    }

    eig::Array3f grid_pos_from_pos(const eig::Array3f &p) const {
      auto clamped_p = p.min(m_space_bounds_max).max(m_space_bounds_min);
      return (m_grid_size).cast<float>() * clamped_p - 0.5f;
    }

  public:
    const eig::Array3i& grid_size() const {
      return m_grid_size;
    } 

    size_t size() const {
      return m_grid_size.prod();
    }
  };

  template <typename T>
  class KNNGrid : public AbstractGrid {
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
    std::vector<std::list<ValueType>>  m_grid;

    uint nearest_index_from_pos(const eig::Array3f &p) const {
      return index_from_grid_pos(grid_pos_from_pos(p).cast<int>());
    }

    std::unordered_set<uint> nearest_indices_from_pos(const eig::Array3f &p) const {
      auto grid_p = grid_pos_from_pos(p);

      // Lower, upper coords determine block of eight nearest grid coordinates
      auto l = grid_p.floor().cast<int>().max(0).eval(),
           u = grid_p.ceil().cast<int>().min(m_grid_size - 1).eval();

      // Gather list of all indices possible with these coordinates
      auto idx = { index_from_grid_pos({ l.x(), l.y(), l.z() }), index_from_grid_pos({ l.x(), l.y(), u.z() }),
                   index_from_grid_pos({ l.x(), u.y(), l.z() }), index_from_grid_pos({ l.x(), u.y(), u.z() }),
                   index_from_grid_pos({ u.x(), l.y(), l.z() }), index_from_grid_pos({ u.x(), l.y(), u.z() }),
                   index_from_grid_pos({ u.x(), u.y(), l.z() }), index_from_grid_pos({ u.x(), u.y(), u.z() }) };

      return std::unordered_set(std::make_move_iterator(idx.begin()),
                                std::make_move_iterator(idx.end()));
    }

  public:
    /* constructors */

    KNNGrid() = default;
    KNNGrid(GridCreateInfo info);

    /* insertion functions */

    void insert_1(const T &t, const eig::Array3f &p);
    void insert_n(std::span<const T> t, std::span<eig::Array3f> p);

    /* query functions */

    QueryType              query_1_nearest(const eig::Array3f &p) const;
    std::vector<QueryType> query_k_nearest(const eig::Array3f &p, uint k);
    std::vector<QueryType> query_n_nearest(const eig::Array3f &p);
  };

  template <typename T>
  class VoxelGrid : public AbstractGrid {
  private:
    std::vector<T> m_grid;

  public:
    /* constructors */

    VoxelGrid() = default;
    VoxelGrid(GridCreateInfo info);

    /* direct accessors */

    const T& at(const eig::Array3i &p) const {
      return m_grid.at(index_from_grid_pos(p));
    }

    T& at(const eig::Array3i &p) {
      return m_grid.at(index_from_grid_pos(p));
    }

    std::span<const T> data() const { return m_grid; }

    /* query functions */

    T query(const eig::Array3f &p) const;
  };
} // namespace met