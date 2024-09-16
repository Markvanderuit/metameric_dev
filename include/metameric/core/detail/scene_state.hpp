#pragma once

#include <metameric/core/math.hpp>
#include <type_traits>
#include <variant>

// Risky; add is_variant_v detector
namespace std {
  template<typename T> struct is_variant : std::false_type {};

  template<typename ...Args>
  struct is_variant<std::variant<Args...>> : std::true_type {};
  
  template<typename T>
  inline constexpr bool is_variant_v = is_variant<T>::value;
} // namespace std

namespace met::detail {
  // Virtual base class for component state tracking in the Scene class
  template <typename Ty>
  class ComponentStateBase {
  protected:
    bool m_mutated = true;

  public:
    // Set the component state as mutated
    constexpr void set_mutated(bool b) { m_mutated = b; }

    // Evaluate known component state
    constexpr bool is_mutated() const { return m_mutated; }
    constexpr operator bool()   const { return m_mutated; };

    // Given a fresh copy of the object, update known component state
    virtual bool update(const Ty &o) = 0;
  };

  // FWD; default component state tracker if no overload is available;
  //      explicit specializations follow below
  template <typename Ty>
  struct ComponentState;

  // Explicit specialization of ComponentState for most types;
  // Stores a full copy of the type which is compared on update()
  template <typename Ty> requires (!std::is_variant_v<Ty>)
  struct ComponentState<Ty> : public ComponentStateBase<Ty> {
    using value_type = Ty;

  private:
    using ComponentStateBase<Ty>::m_mutated;

    value_type m_cache  = { };

  public:
    virtual bool update(const value_type &o) override {
      m_mutated = !eig::safe_approx_compare(m_cache, o);
      if (m_mutated)
        m_cache = o;
      return m_mutated;
    }
  };
  
  // Explicit specialization of ComponentState for std::variant types;
  // Stores a full copy of the variant which is compared on update()
  template <typename Ty> requires (std::is_variant_v<Ty>)
  struct ComponentState<Ty> : public ComponentStateBase<Ty> {
    using value_type = Ty;

  private:
    using ComponentStateBase<Ty>::m_mutated;

    value_type m_cache = { };

  public:
    virtual bool update(const Ty &o) override {
      // Eigen's blocks do not support single-component equality comparison,
      // but in general most things handle this just fine; so hack in here
      if (o.index() != m_cache.index()) {
        m_mutated = true;
      } else if (o.index() == 0) {
        m_mutated = !eig::safe_approx_compare(std::get<0>(o), std::get<0>(m_cache));
      } else if (o.index() == 1) {
        m_mutated = !eig::safe_approx_compare(std::get<1>(o), std::get<1>(m_cache));
      }

      if (m_mutated)
        m_cache = o;
      
      return m_mutated;
    }
  };

  // Wrapper class which instantiates a vector of ComponentState or other
  // for std::vector<Ty> or similar objects
  template <typename Ty, typename State = ComponentState<Ty>>
  requires (std::derived_from<State, ComponentStateBase<Ty>>)
  struct ComponentStates : public ComponentStateBase<std::vector<Ty>> {
    using value_type = Ty;
    using state_type = State;
    using vectr_type = std::vector<value_type>;

  private:
    using ComponentStateBase<vectr_type>::m_mutated;

    std::vector<state_type> m_cache   = { };
    bool                    m_resized = false;

  public:
    // Given a fresh copy of the vector object, update known vector state
    virtual bool update(const std::vector<Ty> &o) override {
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

    // Evaluate known component state
    constexpr bool is_resized() const { return m_resized; }

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
} // namespace met::detail