#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/utility.hpp>

namespace met::detail {
  // Base class for SceneGLHandler and template specializations; only specifies
  // an interface.
  struct SceneGLHandlerBase {
    virtual void update(const Scene &) = 0;
  };

  // Default template implementation; simply does nothing.
  // See scene_components_gl.hpp for component-/resource-specific overloads
  template <typename Ty>
  struct SceneGLHandler : SceneGLHandlerBase {
    void update(const Scene &) override { /* ... */ }
  };

  // Base class for SceneStateHandler and template specializations; only exposes a
  // is_mutated flag and operator bool() that refers to this flag, as well as a
  // interface for the function update().
  template <typename Ty>
  class SceneStateHandlerBase {
  protected:
    bool m_mutated = true;

  public:
    // Set the component state as mutated
    constexpr void set_mutated(bool b) { m_mutated = b; }

    // Evaluate known component state
    constexpr bool is_mutated() const { return m_mutated; }
    constexpr operator bool()   const { return m_mutated; };

    // Children must implement update(), which sets/resets `is_mutated` dependent on data changes
    virtual bool update(const Ty &o) = 0;
  };

  // Base template that implements changed state tracking of any contained type; can be specialized
  // (and is specialized for variants/vectors below). See scene_components_state.hpp for particular
  // component-/resource-specific overloads.
  template <typename Ty>
  class SceneStateHandler : public SceneStateHandlerBase<Ty> {
    using SceneStateHandlerBase<Ty>::m_mutated;

    Ty m_cache  = { };

  public:
    bool update(const Ty &o) override {
      met_trace();
      m_mutated = !eig::safe_approx_compare(m_cache, o); 
      if (m_mutated)
        m_cache = o;
      return m_mutated;
    }
  };

  // Template specialization of SceneStateHandler that handles std::variant
  // and forwards to comparison on underlying types of the same variant
  template <typename Ty> requires (std::is_variant_v<Ty>)
  class SceneStateHandler<Ty> : public SceneStateHandlerBase<Ty> {
    using SceneStateHandlerBase<Ty>::m_mutated;

    Ty m_cache  = { };

  public:
    bool update(const Ty &o) override {
      met_trace();

      // Eigen's blocks do not support single-component equality comparison,
      // but in general most things handle this just fine; so hack in here
      m_mutated = std::visit([](const auto &o, const auto &cache) {
        if constexpr (std::is_same_v<std::remove_reference_t<decltype(o)>,
                                     std::remove_reference_t<decltype(cache)>>)
          return !eig::safe_approx_compare(o, cache);
        else
          return true;
      }, o, m_cache);

      if (m_mutated)
        m_cache = o;
      return m_mutated;
    }
  };

  // Template specialization of SceneStateHandler that handles std::vector<> 
  // and forwards to state tracking inside the vector, and additionally exposes is_resized().
  template <typename Ty, typename Container = SceneStateHandler<Ty>>
  class SceneStateVectorHandler : public SceneStateHandlerBase<std::vector<Ty>> {
    using SceneStateHandlerBase<std::vector<Ty>>::m_mutated;

    std::vector<Container> m_cache = {};
    bool                   m_resized = false;
  
  public:
    constexpr bool is_resized() const { return m_resized; }
    
    bool update(const std::vector<Ty> &o) override {
      met_trace();

      if (m_cache.size() == o.size()) {
        // Handle non-resize case first
        for (uint i = 0; i < m_cache.size(); ++i)
          m_cache[i].update(o[i]);
        m_mutated = false;
        for (const auto &v : m_cache)
          if (v.is_mutated())
            m_mutated = true;
        m_resized = false;
      } else {
        // Handle shrink/grow
        size_t min_r = std::min(m_cache.size(), o.size()), 
               max_r = std::max(m_cache.size(), o.size());
        m_cache.resize(o.size());

        // First compare for smaller range of remaining elements
        for (uint i = 0; i < min_r; ++i)
          m_cache[i].update(o[i]);

        // Potential added elements always have state as 'true'
        if (max_r == o.size())
          for (uint i = min_r; i < max_r; ++i)
            m_cache[i].update(o[i]);
        
        m_mutated = true;
        m_resized = true;
      }
      
      return m_mutated;
    }
  
  public: // Guarded accessors to underlying vector
    // operator[...] exposes throwing at()
    constexpr const auto &operator[](uint i) const { return m_cache.at(i); }
    constexpr       auto &operator[](uint i)       { return m_cache.at(i); }
    
    // Bookkeeping; expose the underlying std::vector, instead of a direct pointer
    constexpr const auto &data() const { return m_cache; }
    constexpr       auto &data()       { return m_cache; }

    // Boilerplate
    constexpr auto size()  const { return m_cache.size();  }
    constexpr auto begin() const { return m_cache.begin(); }
    constexpr auto end()   const { return m_cache.end();   }
    constexpr auto begin()       { return m_cache.begin(); }
    constexpr auto end()         { return m_cache.end();   }
  };
}