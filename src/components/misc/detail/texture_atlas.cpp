#include <metameric/components/misc/detail/texture_atlas.hpp>
#include <algorithm>
#include <deque>

namespace met::detail {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  // Helper function to take a space, and a required size, and return
  // the split result and the set of remainder spaces
  constexpr auto atlas_split = [](auto padded_size, auto space) {
    using Space = decltype(space);

    Space result = { .layer_i = space.layer_i, .offs = space.offs, .size = padded_size };
    std::vector<Space> remainder;

    // Split off horizontal remainder
    if (uint remainder_x = space.size.x() - padded_size.x(); remainder_x > 0)
      remainder.push_back({ .layer_i = space.layer_i,
                            .offs    = space.offs + eig::Array2u(padded_size.x(), 0),
                            .size    = { space.size.x() - padded_size.x(), padded_size.y() } });
    
    // Split off vertical remainder
    if (uint remainder_y = space.size.y() - padded_size.y(); remainder_y > 0)
      remainder.push_back({ .layer_i = space.layer_i,
                            .offs    = space.offs + eig::Array2u(0, padded_size.y()),
                            .size    = { space.size.x(), space.size.y() - padded_size.y() } });

    return std::pair { result, remainder };
  };

  template <typename T, uint D>
  TextureAtlas<T, D>::TextureAtlas(InfoType info)
  : m_method(info.method),
    m_padding(info.padding),
    m_levels(info.levels) {
    met_trace();
    resize(info.sizes);
  }
  
  template <typename T, uint D>
  void TextureAtlas<T, D>::init_views() {
    guard(m_texture.is_init());
    for (uint i = 0; i < m_texture.size().z(); ++i)
      for (uint j = 0; j < m_levels; ++j)
        m_texture_views.push_back({{ .texture = &m_texture, .min_level = j, .min_layer = i }});
  }

  template <typename T, uint D>
  void TextureAtlas<T, D>::dstr_views() {
    m_texture_views.clear();
  }

  template <typename T, uint D>
  void TextureAtlas<T, D>::resize(vec2 size, uint count) {
    met_trace_full();
    std::vector<vec2> v(count, size);
    resize(v);
  }

  template <typename T, uint D>
  void TextureAtlas<T, D>::clear() {
    met_trace_full();
    m_patches.clear();
    m_free.resize(m_texture.size().z());
    for (uint i = 0; i < m_free.size(); ++i)
      m_free[i] = { .layer_i = i, .offs = 0, .size = m_texture.size().head<2>() };
  }

  template <typename T, uint D>
  void TextureAtlas<T, D>::reserve(vec3 size) {
    met_trace_full();
    guard((capacity() < size).any());
    dstr_views();
    m_texture = {{ .size = size, .levels = m_levels }};
    init_views();
  }

  template <typename T, uint D>
  void TextureAtlas<T, D>::reserve_buffer(size_t size) {
    met_trace_full();
    size_t buffer_size = static_cast<size_t>(size) * sizeof(PatchLayout);
    guard(!m_buffer.is_init() || m_buffer.size() >= buffer_size);

    m_buffer_map = { };
    m_buffer     = {{ .size = buffer_size, .flags = buffer_create_flags }};
    m_buffer_map = m_buffer.map_as<PatchLayout>(buffer_access_flags);
  }
  
  template <typename T, uint D>
  void TextureAtlas<T, D>::resize(std::span<vec2> sizes) {
    met_trace_full();

    if (sizes.empty()) {
      clear();
      return;
    }

    // Determine maximum horizontal/vertical patch size plus potential padding
    vec2 max_size = {
      rng::max(sizes, {}, [](auto v) { return v.x(); }).x() + 2 * m_padding,
      rng::max(sizes, {}, [](auto v) { return v.y(); }).y() + 2 * m_padding
    };
    
    // Establish necessary capacity based on existing texture's size, or size of the maximum
    // h/v patch size if capacity is already insufficient
    vec3 new_capacity = (vec3() << capacity().head<2>().max(max_size), 1).finished();

    // Generate work data; copy all input sizes, assign them an index, and sort them by area
    struct Work { uint i; vec2 size; };
    std::vector<Work> work_data(sizes.size());
    for (uint i = 0; i < sizes.size(); ++i)
      work_data[i] = { .i = i, .size = sizes[i] };
    rng::sort(work_data, rng::greater {}, [](const auto &work) { return work.size.prod(); });

    // Define a work queue of sorted input sizes
    std::deque<Work> work_queue;
    work_queue.assign_range(work_data);

    // Helper capture to reset state, generally called after the
    // texture's estimated capacity is grown as a fit could not be found
    auto perform_init = [&]() {
      // Reset reservations data
      m_patches.clear();
      m_patches.resize(sizes.size());
      
      // Reset remainder data, incorporating potential texture growth
      m_free.resize(new_capacity.z());
      for (uint i = 0; i < m_free.size(); ++i)
        m_free[i] = { .layer_i = i, .offs = 0, .size = new_capacity.head<2>() };
        
      // Reset work queue
      work_queue.assign_range(work_data);
    };

    // Helper capture to grow the texture's estimated necessary capacity
    // following the specified growth method
    auto perform_grow = [&]() {
      switch (m_method) {
        case BuildMethod::eLayered:
          new_capacity.z()++;
          break;
        case BuildMethod::eSpread:
          new_capacity.head<2>() *= 2;
          break;
        default: 
          debug::check_expr(false);
      };
    };
    
    // Iterate work queue until empty
    perform_init();
    while (!work_queue.empty()) {
      // Get index, size of next patch
      auto [i, size] = work_queue.front();
      
      // Actual size of a reserved patch includes user-specified padding around it
      size = size + m_padding * 2;

      // Generate a view over empty spaces where the current work fits
      // If no suitable empty space was found, we grow the estimated capacity and restart
      auto candidates = vws::filter(m_free, [&](auto s) { return (size <= s.size).all(); });
      if (candidates.empty()) {
        perform_grow();
        perform_init();
        continue;
      }

      // Find the smallest available space for the current work
      auto candidate_it = rng::min_element(candidates, {}, [](auto s) { return s.size.prod(); });

      // Split canndidate into parts, and store remainder(s) as empty space for later reservation
      auto [patch, remainder] = atlas_split(size, *candidate_it);
      m_free.erase(candidate_it.base());
      m_free.insert(m_free.end(), range_iter(remainder));

      // Strip padding from the reserved space, and store it at the matching index
      patch.offs += m_padding;
      patch.size -= m_padding * 2;
      m_patches[i] = patch;

      work_queue.pop_front();
    } // while (!work_queue.empty())

    // Ensure the underlying storage is suitable for the current set of patches
    reserve(new_capacity);
    reserve_buffer(m_patches.size());
    
    // Copy patch layouts to buffer storage
    rng::copy(m_patches, m_buffer_map.begin());
    m_buffer.flush(m_patches.size() * sizeof(PatchLayout));
  }

  /* template <typename T, uint D>
  void TextureAtlas<T, D>::shrink_to_fit() {
    guard(m_texture.is_init());
    met_trace_full();

    // Clear out view objects on potential reallocates
    m_texture_views.clear();

    // Determine the maximum size necessary for all reserved space
    auto view = m_resrv | vws::transform([&](const auto &s) { return s.offs + s.size + m_padding; });
    auto maxm = rng::fold_left(view, vec2(0), [](auto a, auto b) { return a.cwiseMax(b).eval();  });
    auto maxl = 1 + rng::max(m_resrv, {}, &PatchLayout::layer_i).layer_i;

    // Allocate the resulting texture
    m_texture = {{ .size   = eig::Array3u { maxm.x(), maxm.y(), maxl }, .levels = m_texture.levels() }};

    // Generate relevant view objects
    for (uint i = 0; i < m_texture.size().z(); ++i)
      for (uint j = 0; j < m_texture.levels(); ++j)
        m_texture_views.push_back({{ .texture   = &m_texture, .min_level = j, .min_layer = i }});
  }
 */
  /* Explicit template instantiations */

  #define met_explicit_atlas(type, components) template class TextureAtlas<type, components>;
    
  #define met_explicit_atlas_components(type)    \
    met_explicit_atlas(type, 1)                  \
    met_explicit_atlas(type, 2)                  \
    met_explicit_atlas(type, 3)                  \
    met_explicit_atlas(type, 4)

  #define met_explicit_atlas_types()             \
    met_explicit_atlas_components(ushort)        \
    met_explicit_atlas_components(short)         \
    met_explicit_atlas_components(uint)          \
    met_explicit_atlas_components(int)           \
    met_explicit_atlas_components(float)

  met_explicit_atlas_types()
} // namespace met::detail