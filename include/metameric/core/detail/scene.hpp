#pragma once

#include <functional>

namespace met::detail {
  /* Virtual base class for scene component state tracking. */
  template <typename Ty>
  class ComponentStateBase {
  protected:
    bool m_stale = true;

  public:
    constexpr
    void set_stale(bool b) { m_stale = b; }

    constexpr
    bool is_stale() const { return m_stale; }
    
    constexpr 
    operator bool() const { return m_stale; };

    virtual 
    bool update_state(const Ty &o) = 0;
  };

  /* Default implementing class for component state tracking;
      simply stores single state for the entire component. It's
      either changed, or not changed. */
  template <typename Ty>
  class ComponentState : public ComponentStateBase<Ty> {
    using ComponentStateBase<Ty>::m_stale;
    Ty m_cache  = { };

  public:
    virtual 
    bool update_state(const Ty &o) override {
      // Eigen's blocks do not support single-component equality comparison,
      // but in general most things handle this just fine; so hack in here
      if constexpr (is_approx_comparable<Ty>)
        m_stale = !m_cache.isApprox(o);
      else
        m_stale = m_cache != o;

      if (m_stale)
        m_cache = o;
      return m_stale;
    }
  };

  template <typename Ty, typename State = ComponentState<Ty>>
  requires (std::derived_from<State, ComponentStateBase<Ty>>)
  class VectorState : public ComponentStateBase<std::vector<Ty>> {
    using ComponentStateBase<std::vector<Ty>>::m_stale;
    std::vector<State> m_cache  = { };

  public:
    virtual 
    bool update_state(const std::vector<Ty> &o) override {
      if (m_cache.size() == o.size()) {
        // Handle non-resize case first
        for (uint i = 0; i < m_cache.size(); ++i)
          m_cache[i].update_state(o[i]);
        m_stale = rng::any_of(m_cache, [](const auto &v) { return v.is_stale(); });
      } else {
        // Handle shrink/grow
        size_t min_r = std::min(m_cache.size(), o.size()), 
                max_r = std::max(m_cache.size(), o.size());
        m_cache.resize(o.size());

        // First compare for smaller range of remaining elements
        for (uint i = 0; i < min_r; ++i)
          m_cache[i].update_state(o[i]);

        // Potential added elements always have state as 'true'
        if (max_r == o.size())
          for (uint i = min_r; i < max_r; ++i)
            m_cache[i].update_state(o[i]);
        
        m_stale = true;
      }

      return m_stale;
    }
  };
} // namespace met::detail