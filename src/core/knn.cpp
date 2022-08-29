#include <metameric/core/knn.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/detail/trace.hpp>
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
    constexpr bool query_comp(const typename KNNGrid<T>::QueryType &a, 
                              const typename KNNGrid<T>::QueryType &b) { 
      return a.distance < b.distance; 
    }
    

    template <typename T>
    typename KNNGrid<T>::QueryType value_to_query(const typename KNNGrid<T>::ValueType &v, 
                                                  const eig::Array3f &p) {
      return { v.position, v.value, sq_euql_dist(v.position, p) };
    }

    template <typename T>
    T lerp(const T &v1, const T &v2, const auto &a) {
      return v1 + a * (v2 - v1);
    }
  } // namespace detail

  AbstractGrid::AbstractGrid(GridCreateInfo info)
  : m_grid_size(info.grid_size),
    m_space_bounds_min(info.space_bounds_min),
    m_space_bounds_max(info.space_bounds_max),
    m_max_cell_size(info.max_cell_size) { }

  template <typename T>
  VoxelGrid<T>::VoxelGrid(GridCreateInfo info)
  : AbstractGrid(info),
    m_grid(info.grid_size.prod()) { 
    met_trace();
    met_trace_alloc(m_grid.data(), m_grid.size() * sizeof(decltype(m_grid)::value_type));
  }

  template <typename T>
  VoxelGrid<T>::~VoxelGrid() {
    met_trace();
    met_trace_free(m_grid.data());
  }

  template <typename T>
  T VoxelGrid<T>::query(const eig::Array3f &p) const {
    met_trace();

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
    m_grid(m_grid_size.prod()) {
    met_trace();
    met_trace_alloc(m_grid.data(), m_grid.size() * sizeof(decltype(m_grid)::value_type));
  }

  template <typename T>
  KNNGrid<T>::~KNNGrid() {
    met_trace();
    met_trace_free(m_grid.data());
  }

  template <typename T>
  void KNNGrid<T>::insert_n(std::span<const T> t, std::span<eig::Array3f> p) {
    met_trace();

    // Generate indices to iterate both t and p
    std::vector<uint> indices(p.size());
    std::iota(range_iter(indices), 0);

    // Generate grid of mutex locks to perform parallel list insertion
    std::vector<std::mutex> lock_grid(m_grid_size.prod());

    // Perform parallel iteration
    std::for_each(std::execution::par, range_iter(indices), [&](uint i) {
      const auto &p_i = p[i];
      const auto &t_i = t[i];
      uint j = nearest_index_from_pos(p_i);

      // Obtain a lock on voxel position and push data on to list
      std::lock_guard lock(lock_grid[j]);
      if (m_max_cell_size == -1 || m_grid[j].size() < m_max_cell_size) {
        m_grid[j].push_back({ p_i, t_i });
      }
    });
  }

  template <typename T>
  void KNNGrid<T>::insert_1(const T &t, const eig::Array3f &p) {
    met_trace();

    m_grid[nearest_index_from_pos(p)].push_back({ p, t });
  }
  
  template <typename T>
  KNNGrid<T>::QueryType KNNGrid<T>::query_1_nearest(const eig::Array3f &p) const {
    met_trace();

    // Construct search list of points in nearest grid cells
    auto v_to_query = std::bind(detail::value_to_query<T>, std::placeholders::_1, std::cref(p));
    auto query_view = nearest_indices_from_pos(p) 
                    | std::views::transform([&](uint i) -> const std::vector<ValueType>& { return m_grid[i]; })
                    | std::views::join 
                    | std::views::transform(v_to_query);

    // Gather transformed queries from search list
    std::vector<QueryType> query_list;
    query_list.reserve(m_max_cell_size == -1 ? 128ul : m_max_cell_size * 8);
    std::ranges::copy(query_view, std::back_inserter(query_list));

    // Return a false query on an empty list
    guard(query_list.size(), { 0.f, 0.f, FLT_MAX });

    // Perform search for the closest query and return it
    return *std::min_element(std::execution::par_unseq, 
      range_iter(query_list), detail::query_comp<T>);
  }

  template <typename T>
  std::vector<typename KNNGrid<T>::QueryType> 
  KNNGrid<T>::query_k_nearest(const eig::Array3f &p, uint k) {
    met_trace();

    // Stupidity check for k==1 to just return the nearest element
    guard(k > 1, { query_1_nearest(p) });

    // Construct search list of points in nearest grid cells
    auto v_to_query = std::bind(detail::value_to_query<T>, std::placeholders::_1, std::cref(p));
    auto query_view = nearest_indices_from_pos(p) 
                    | std::views::transform([&](uint i) -> const std::vector<ValueType>& { return m_grid[i]; })
                    | std::views::join 
                    | std::views::transform(v_to_query);

    // Convert to query objects and sort by distnace
    std::vector<QueryType> query_list;
    query_list.reserve(m_max_cell_size == -1 ? 128ul : m_max_cell_size * 8);
    std::ranges::copy(query_view, std::back_inserter(query_list));
    std::ranges::sort(query_list,  detail::query_comp<T>);

    // Resize to keep nearest k results
    query_list.resize(std::min<size_t>(k, query_list.size()));

    return query_list;
  }

  template <typename T>
  std::vector<typename KNNGrid<T>::QueryType> 
  KNNGrid<T>::query_n_nearest(const eig::Array3f &p) {
    met_trace();

    // Construct search list of points in nearest grid cells
    auto v_to_query = std::bind(detail::value_to_query<T>, std::placeholders::_1, std::cref(p));
    auto query_view = nearest_indices_from_pos(p) 
                    | std::views::transform([&](uint i) -> const std::vector<ValueType>& { return m_grid[i]; })
                    | std::views::join 
                    | std::views::transform(v_to_query);

    // Convert to query objects and sort by distnace
    std::vector<QueryType> queries;
    std::ranges::copy(query_view, std::back_inserter(queries));
    std::ranges::sort(queries,  detail::query_comp<T>);
    
    return queries;
  }

  template <typename T>
  void KNNGrid<T>::retrace_size() {
#ifdef MET_ENABLE_TRACY
    auto size_v = m_grid | std::views::transform([](auto &v) { return v.size() * sizeof(T); });
    size_t size = std::reduce(range_iter(size_v));
    met_trace_realloc(m_grid.data(), size);
#endif // MET_ENABLE_TRACY
  }

  /* Explicit template instantiations of met::VoxelGrid<T> and met::KNNGrid<T> */

  template class VoxelGrid<float>;
  template class VoxelGrid<Spec>;
  template class KNNGrid<float>;
  template class KNNGrid<Spec>;
} // namespace met