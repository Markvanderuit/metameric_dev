#include <metameric/core/ranges.hpp>
#include <metameric/core/atlas.hpp>
#include <algorithm>
#include <deque>

namespace met {
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
    std::tie(m_buffer, m_buffer_map) = gl::Buffer::make_flusheable_object<AtlasBufferLayout>();
    resize(info.sizes);
  }
  
  template <typename T, uint D>
  void TextureAtlas<T, D>::init_views() {
    guard(m_texture.is_init());
    for (uint i = 0; i < capacity().z(); ++i)
      for (uint j = 0; j < m_levels; ++j)
        m_texture_views.push_back({{ .texture = &m_texture, .min_level = j, .min_layer = i }});
  }

  template <typename T, uint D>
  void TextureAtlas<T, D>::dstr_views() {
    m_texture_views.clear();
  }

  template <typename T, uint D>
  void TextureAtlas<T, D>::resize(eig::Array2u size, uint count) {
    met_trace_full();
    std::vector<eig::Array2u> v(count, size);
    resize(v);
  }

  template <typename T, uint D>
  void TextureAtlas<T, D>::clear() {
    met_trace_full();
    m_patches.clear();
    m_free.resize(capacity().z());
    for (uint i = 0; i < m_free.size(); ++i)
      m_free[i] = { .layer_i = i, .offs = 0, .size = capacity().head<2>() };
    m_is_invalitated = true;
  }

  template <typename T, uint D>
  void TextureAtlas<T, D>::reserve(eig::Array3u new_capacity) {
    met_trace_full();
    
    // Only grow if necessary. texture.shrink_to_fit() handles, well, shrinking
    guard((capacity() < new_capacity).any());
    
    dstr_views();
    m_texture        = {{ .size = new_capacity, .levels = m_levels }};
    m_is_invalitated = true;
    init_views();
  }

  template <typename T, uint D>
  eig::Array3u TextureAtlas<T, D>::capacity() const { 
    return m_texture.is_init() ? m_texture.size() : 0;
  }
  
  template <typename T, uint D>
  void TextureAtlas<T, D>::resize(std::span<eig::Array2u> sizes) {
    met_trace_full();

    m_is_invalitated = false;

    // Define cleared replacements for m_patches, m_free
    std::vector<AtlasBlockLayout> new_patches, new_free(capacity().z());
    for (uint i = 0; i < new_free.size(); ++i)
      new_free[i] = { .layer_i = i, .offs = 0, .size = capacity().head<2>() };

    if (sizes.empty()) {
      clear();
      return;
    }

    // Determine maximum horizontal/vertical patch size plus potential padding
    eig::Array2u max_size = { rng::max(sizes, {}, [](auto v) { return v.x(); }).x() + 2 * m_padding,
                              rng::max(sizes, {}, [](auto v) { return v.y(); }).y() + 2 * m_padding };
    
    // Establish necessary capacity based on existing texture's size, or size of the maximum
    // h/v patch size if capacity is already insufficient
    auto new_capacity = (eig::Array3u() << capacity().head<2>().max(max_size), 1).finished();

    // Generate work data; copy all input sizes, assign them an index, and sort them by area
    struct Work { uint i; eig::Array2u size; };
    std::vector<Work> work_data(sizes.size());
    for (uint i = 0; i < sizes.size(); ++i)
      work_data[i] = { .i = i, .size = sizes[i] };
    rng::sort(work_data, rng::greater {}, [](const auto &work) { return work.size.prod(); });

    // Define a work queue of sorted input sizes
    std::deque<Work> work_queue;

    // Capture to reset state, generally called after the
    // texture's estimated capacity is grown as a fit could not be found
    auto perform_init = [&]() {
      // Reset reservations data
      new_patches.clear();
      new_patches.resize(sizes.size());
      
      // Reset remainder data, incorporating potential texture growth
      new_free.resize(new_capacity.z());
      for (uint i = 0; i < new_free.size(); ++i)
        new_free[i] = { .layer_i = i, .offs = 0, .size = new_capacity.head<2>() };
        
      // Reset work queue
      work_queue.assign_range(work_data);
    };

    // Helper capture to grow the texture's estimated necessary capacity
    // following the specified growth method
    auto perform_grow = [&](eig::Array2u size) {
      switch (m_method) {
        case BuildMethod::eLayered:
          new_capacity.z()++;
          break;
        case BuildMethod::eSpread:
          if (new_capacity.x() > new_capacity.y())
            new_capacity.y() += size.y();
          else
            new_capacity.x() += size.x();
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
      auto candidates = vws::filter(new_free, [&](auto s) { return (size <= s.size).all(); });
      if (candidates.empty()) {
        perform_grow(size);
        perform_init();
        continue;
      }

      // Find the smallest available space for the current work
      auto candidate_it = rng::min_element(candidates, {}, [](auto s) { return s.size.prod(); });

      // Split canndidate into parts, and store remainder(s) as empty space for later reservation
      auto [patch, remainder] = atlas_split(size, *candidate_it);
      new_free.erase(candidate_it.base());
      new_free.insert(new_free.end(), range_iter(remainder));

      // Strip padding from the reserved space, and store it at the matching index
      patch.offs += m_padding;
      patch.size -= m_padding * 2;
      new_patches[i] = patch;

      work_queue.pop_front();
    } // while (!work_queue.empty())

    // Tightly pack capacity for the current required space
    auto view = new_patches | vws::transform([&](const auto &s) { return s.offs + s.size + m_padding; });
    auto maxm = rng::fold_left(view, eig::Array2u(0), [](auto a, auto b) { return a.cwiseMax(b).eval();  });
    auto maxl = 1 + rng::max(new_patches, {}, &AtlasBlockLayout::layer_i).layer_i;
    new_capacity = { maxm.x(), maxm.y(), maxl };

    // Ensure the underlying storage is suitable for the current set of patches
    reserve(new_capacity);

    // Test if the new patches are simply an expansion, in which case the current underlying
    // memory is not invalidated unless texture.reserve() handles this
    if (new_patches.size() >= m_patches.size()) {
      auto overlap_eq = rng::equal(std::span(new_patches).subspan(0, m_patches.size()), 
                                   std::span(m_patches).subspan(0,   m_patches.size()), 
      [](const AtlasBlockLayout &a, const AtlasBlockLayout &b) {
        return a.layer_i == b.layer_i && a.offs.isApprox(b.offs) && a.size.isApprox(b.size);
      });
      if (!overlap_eq) {
        m_is_invalitated = true;
      }
    }

    // Update uv0/uv1 values in the patch data
    // these make texture lookups slightly faster
    for (auto &patch : new_patches) {
      patch.uv0 = patch.offs.cast<float>() / m_texture.size().head<2>().cast<float>();
      patch.uv1 = patch.size.cast<float>() / m_texture.size().head<2>().cast<float>();
    }
    
    // Store new patches
    m_patches = new_patches;
    m_free    = new_free;

    // Copy patch layouts to buffer storage
    m_buffer_map->size = new_patches.size();
    rng::copy(new_patches, m_buffer_map->data.begin());
    m_buffer.flush();
  }

  template <typename T, uint D>
  void TextureAtlas<T, D>::shrink_to_fit() {
    guard(m_texture.is_init());
    met_trace_full();

    // Tightly pack capacity for the current required space
    auto view = m_patches | vws::transform([&](const auto &s) { return s.offs + s.size + m_padding; });
    auto maxm = rng::fold_left(view, eig::Array2u(0), [](auto a, auto b) { return a.cwiseMax(b).eval();  });
    auto maxl = 1 + rng::max(m_patches, {}, &AtlasBlockLayout::layer_i).layer_i;
    eig::Array3u new_capacity = { maxm.x(), maxm.y(), maxl };
    
    dstr_views();

    // Allocate the new underlying texture
    TextureArray texture = {{ .size = new_capacity, .levels = m_levels }};

    // Copy over the new_texture's overlap from m_texture, then swap objects
    m_texture.copy_to(texture, 0, texture.size());
    m_texture.swap(texture);

    init_views();

    // Update uv0/uv1 values in the patch data
    for (auto &patch : m_patches) {
      patch.uv0 = patch.offs.cast<float>() / m_texture.size().head<2>().cast<float>();
      patch.uv1 = patch.size.cast<float>() / m_texture.size().head<2>().cast<float>();
    }
    
    // Copy updated patch layouts to buffer storage
    m_buffer_map->size = m_patches.size();
    rng::copy(m_patches, m_buffer_map->data.begin());
    m_buffer.flush();
  }

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
} // namespace met