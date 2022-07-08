#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/spectral_grid.hpp>
#include <algorithm>
#include <execution>
#include <functional>
#include <mutex>
#include <ranges>

namespace met {
  namespace detail {
    float sq_euql_dist(const eig::Array3f &a, const eig::Array3f &b) {
      return (b - a).pow(2).sum();
    }
    
    float eucl_dist(const eig::Array3f &a, const eig::Array3f &b) {
      return std::sqrtf(sq_euql_dist(a, b));
    }

    template <typename T>
    typename KNNGrid<T>::QueryType value_to_query(const typename KNNGrid<T>::ValueType &v, 
                                                  const eig::Array3f &p) {
      return { v.position, v.value, eucl_dist(v.position, p) };
    };

    template <typename T>
    T lerp(const T &v1, const T &v2, const auto &a) {
      return v1 + a * (v2 - v1);
    }
  } // namespace detail

  AbstractGrid::AbstractGrid(GridCreateInfo info)
  : m_grid_size(info.grid_size),
    m_space_bounds_min(info.space_bounds_min),
    m_space_bounds_max(info.space_bounds_max) { }

  template <typename T>
  VoxelGrid<T>::VoxelGrid(GridCreateInfo info)
  : AbstractGrid(info),
    m_grid(info.grid_size.prod()) { }

  template <typename T>
  T VoxelGrid<T>::query(const eig::Array3f &p) const {
    auto grid_p = grid_pos_from_pos(p);

    // Lower, upper, and alpha coords for trilinear interpolation
    auto l = grid_p.floor().cast<int>().max(0).eval(),
         u = grid_p.ceil().cast<int>().min(m_grid_size - 1).eval();
    auto a = (grid_p - grid_p.floor()).eval();
    
    // Perform trilinear interpolation
    auto ll = detail::lerp(at({ l.x(), l.y(), l.z() }), at({ l.x(), l.y(), u.z() }), a.z());
    auto lu = detail::lerp(at({ l.x(), u.y(), l.z() }), at({ l.x(), u.y(), u.z() }), a.z());
    auto ul = detail::lerp(at({ u.x(), l.y(), l.z() }), at({ u.x(), l.y(), u.z() }), a.z());
    auto uu = detail::lerp(at({ u.x(), u.y(), l.z() }), at({ u.x(), u.y(), u.z() }), a.z());
    return detail::lerp(detail::lerp(ll, lu, a.y()), detail::lerp(ul, uu, a.y()), a.x());
  }

  template <typename T>
  KNNGrid<T>::KNNGrid(GridCreateInfo info)
  : AbstractGrid(info),
    m_grid(m_grid_size.prod()) { }

  template <typename T>
  void KNNGrid<T>::insert_n(std::span<const T> t, std::span<eig::Array3f> p) {
    // Generate indices to iterate both t and p
    std::vector<uint> indices(p.size());
    std::iota(indices.begin(), indices.end(), 0);

    // Generate grid of mutex locks to perform parallel list insertion
    std::vector<std::mutex> lock_grid(m_grid_size.prod());

    // Perform parallel iteration
    std::for_each(std::execution::par, indices.begin(), indices.end(), [&](uint i) {
      const auto &p_i = p[i];
      const auto &t_i = t[i];
      uint j = nearest_index_from_pos(p_i);

      // Obtain a lock on voxel position and push data on to list
      std::lock_guard lock(lock_grid[j]);
      m_grid[j].push_back({ p_i, t_i });
    });
  }

  template <typename T>
  void KNNGrid<T>::insert_1(const T &t, const eig::Array3f &p) {
    m_grid[nearest_index_from_pos(p)].push_back({ p, t });
  }
  
  template <typename T>
  KNNGrid<T>::QueryType KNNGrid<T>::query_1_nearest(const eig::Array3f &p) const {
    // Construct range as search list of points in 8 nearest grid cells
    auto to_query = std::bind(detail::value_to_query<T>, std::placeholders::_1, std::cref(p));
    auto query_view = nearest_indices_from_pos(p) 
                    | std::views::transform([&](uint i) { return m_grid[i]; })
                    | std::views::join
                    | std::views::transform(to_query);

    // Perform linear search for the closest position
    QueryType query = { eig::Array3f::Zero(), 0.f, FLT_MAX };
    for (const QueryType &q : query_view) {
      if (q.distance < query.distance) {
        query = q;
      }
    }
    
    return query;
  }

  template <typename T>
  std::vector<typename KNNGrid<T>::QueryType> 
  KNNGrid<T>::query_k_nearest(const eig::Array3f &p, uint k) {
    // Stupidity check for k==1 to just return the nearest element
    guard(k > 1, { query_1_nearest(p) });

    // Construct range as search list of points in 8 nearest grid cells
    auto to_query = std::bind(detail::value_to_query<T>, std::placeholders::_1, std::cref(p));
    auto query_view = nearest_indices_from_pos(p) 
                    | std::views::transform([&](uint i) { return m_grid[i]; })
                    | std::views::join
                    | std::views::transform(to_query);

    // Convert to query objects and sort by distnace
    std::vector<QueryType> queries;
    std::ranges::copy(query_view, std::back_inserter(queries));
    std::ranges::sort(queries, [](const auto &a, const auto &b) { return a.distance < b.distance; });

    // Resize to keep nearest k results
    queries.resize(std::min<size_t>(k, queries.size()));

    return queries;
  }

  
  template <typename T>
  std::vector<typename KNNGrid<T>::QueryType> 
  KNNGrid<T>::query_n_nearest(const eig::Array3f &p) {
    // Construct range as search list of points in 8 nearest grid cells
    auto to_query = std::bind(detail::value_to_query<T>, std::placeholders::_1, std::cref(p));
    auto query_view = nearest_indices_from_pos(p) 
                    | std::views::transform([&](uint i) { return m_grid[i]; })
                    | std::views::join
                    | std::views::transform(to_query);

    // Convert to query objects and sort by distnace
    std::vector<QueryType> queries;
    std::ranges::copy(query_view, std::back_inserter(queries));
    std::ranges::sort(queries, [](const auto &a, const auto &b) { return a.distance < b.distance; });
    
    return queries;
  }

  /* Explicit template instantiations of met::VoxelGrid<T> and met::KNNGrid<T> */

  template class VoxelGrid<float>;
  template class VoxelGrid<Spec>;
  template class KNNGrid<float>;
  template class KNNGrid<Spec>;
} // namespace met