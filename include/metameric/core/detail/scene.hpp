#pragma once

#include <metameric/core/utility.hpp>
#include <functional>
#include <type_traits>

namespace met::detail {
  /* Virtual base class for component state tracking in the
     app's Scene class. */
  template <typename Ty>
  class ComponentStateBase {
  protected:
    bool m_mutated = true;

  public:
    constexpr
    void set_mutated(bool b) { m_mutated = b; }

    constexpr
    bool is_mutated() const { return m_mutated; }
    
    constexpr 
    operator bool() const { return m_mutated; };

    virtual 
    bool update_state(const Ty &o) = 0;
  };

  /* Default implementing class for component state tracking;
     simply stores single state for the entire component. It's
     either changed, or not changed. */
  template <typename Ty>
  class ComponentState : public ComponentStateBase<Ty> {
    using ComponentStateBase<Ty>::m_mutated;
    Ty m_cache  = { };

  public:
    virtual 
    bool update_state(const Ty &o) override {
      // Eigen's blocks do not support single-component equality comparison,
      // but in general most things handle this just fine; so hack in here
      if constexpr (is_approx_comparable<Ty>)
        m_mutated = !m_cache.isApprox(o);
      else
        m_mutated = m_cache != o;

      if (m_mutated)
        m_cache = o;
      
      return m_mutated;
    }
  };

  /* Wrapper class for component state tracking; tracks
     state across a vector of components, and handles
     resizes of the vector gracefully */
  template <typename Ty, typename State = ComponentState<Ty>>
  requires (std::derived_from<State, ComponentStateBase<Ty>>)
  class ComponentStateVector : public ComponentStateBase<std::vector<Ty>> {
    using ComponentStateBase<std::vector<Ty>>::m_mutated;
    std::vector<State> m_cache  = { };

  public:
    virtual 
    bool update_state(const std::vector<Ty> &o) override {
      if (m_cache.size() == o.size()) {
        // Handle non-resize case first
        for (uint i = 0; i < m_cache.size(); ++i)
          m_cache[i].update_state(o[i]);
        m_mutated = rng::any_of(m_cache, [](const auto &v) { return v.is_mutated(); });
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
        
        m_mutated = true;
      }

      return m_mutated;
    }
  };

  /* Scene component.
     Wrapper around objects/emitters/etc present in the scene, to handle component name, active flag,
     and specializable state tracking to detect internal changes for e.g. the Uplifting object. */
  template <typename Ty,
            typename State = ComponentState<Ty>>
            requires (std::derived_from<State, ComponentStateBase<Ty>>)
  struct Component {
    bool        is_active = true;
    std::string name      = "";
    Ty          value     = { };
    State       state     = { };

    constexpr friend 
    auto operator<=>(const Component &, const Component &) = default;
  };

  /* Scene resource.
     Wrapper around meshes/textures/spectra used by components in the scene, to handle resource
     name, and especially simple state tracking without storing a resource duplicate. */
  template <typename Ty>
  class Resource {
    bool m_mutated = true;
    Ty   m_value   = { };

  public:
    std::string name = "";

    Resource() = default;
    Resource(std::string_view name, const Ty &value) 
    : name(name), m_value(value) { }
    Resource(std::string_view name, Ty &&value) 
    : name(name), m_value(std::move(value)) { }

  public:
    constexpr void set_mutated(bool b) { m_mutated = b; }
    constexpr bool is_mutated() const { return m_mutated; }

    constexpr const Ty &value() const { return m_value; }
    constexpr       Ty &value()       { set_mutated(true); return m_value; }

    constexpr friend 
    auto operator<=>(const Resource &, const Resource &) = default;
  };

  /* Scene component vector.
     Encapsulates std::vector<Component<Ty>> to handle named component lookups and some minor
     syntactic sugar for easy component initialization. */
  template <typename Ty,
            typename State = ComponentState<Ty>>
  class ComponentVector {
    using Comp = Component<Ty, State>;
    std::vector<Comp> m_data;

  public:
    constexpr void push(std::string_view name, const Ty &value) {
      m_data.push_back(Comp { .name = std::string(name), .value = value });
    }

    constexpr void emplace(std::string_view name, Ty &&value) {
      m_data.emplace_back(Comp { .name = std::string(name), .value = std::move(value) });
    }
    
    constexpr void erase(std::string_view name) {
      auto it = rng::find(m_data, name, &Comp::name);
      debug::check_expr(it != m_data.end(), "Erased scene component does not exist");
      m_data.erase(it);
    }

    // operator[...] exposes throwing at()
    constexpr const Comp &operator[](uint i) const { return m_data.at(i); }
    constexpr       Comp &operator[](uint i)       { return m_data.at(i); }

    // operator("...") enables named lookup
    constexpr const Comp &operator()(std::string_view name) const {
      auto it = rng::find(m_data, name, &Comp::name);
      debug::check_expr(it != m_data.end(), "Queried scene component does not exist");
      return *it;
    }
    constexpr       Comp &operator()(std::string_view name)       {
      auto it = rng::find(m_data, name, &Comp::name);
      debug::check_expr(it != m_data.end(), "Queried scene component does not exist");
      return *it;
    }

    // Bookkeeping; expose the underlying std::vector, instead of a direct pointer
    constexpr const auto & data() const { return m_data; }
    constexpr       auto & data()       { return m_data; }

    // Bookkeeping; expose miscellaneous std::vector member functions
    constexpr void resize(size_t i) { m_data.resize(i);      }
    constexpr void erase(uint i)    { m_data.erase(i);       }
    constexpr void clear()          { m_data.clear();        }
    constexpr auto size()     const { return m_data.size();  }
    constexpr auto begin()    const { return m_data.begin(); }
    constexpr auto end()      const { return m_data.end();   }
    constexpr auto begin()          { return m_data.begin(); }
    constexpr auto end()            { return m_data.end();   }
  };

  /* Scene resource vector.
     Encapsulates std::vector<Resource<Ty>> to handle named resource lookups and some minor
     syntactic sugar for easy resource initialization. */
  template <typename Ty>
  class ResourceVector {
    using Rsrc = Resource<Ty>;
    std::vector<Rsrc> m_data;

  public:
    constexpr void push(std::string_view name, const Ty &value) {
      m_data.push_back(Rsrc(std::string(name), value));
    }

    constexpr void emplace(std::string_view name, Ty &&value) {
      m_data.emplace_back(Rsrc(std::string(name), std::move(value)));
    }
    
    constexpr void erase(std::string_view name) {
      auto it = rng::find(m_data, name, &Rsrc::name);
      debug::check_expr(it != m_data.end(), "Erased scene resource does not exist");
      m_data.erase(it);
    }

    // operator[...] exposes throwing at()
    constexpr const Rsrc &operator[](uint i) const { return m_data.at(i); }
    constexpr       Rsrc &operator[](uint i)       { return m_data.at(i); }

    // operator("...") enables named lookup
    constexpr const Rsrc &operator()(std::string_view name) const {
      auto it = rng::find(m_data, name, &Rsrc::name);
      debug::check_expr(it != m_data.end(), "Queried scene resource does not exist");
      return *it;
    }
    constexpr       Rsrc &operator()(std::string_view name)       {
      auto it = rng::find(m_data, name, &Rsrc::name);
      debug::check_expr(it != m_data.end(), "Queried scene resource does not exist");
      return *it;
    }

    // Bookkeeping; expose the underlying std::vector, instead of a direct pointer
    constexpr const auto & data() const { return m_data; }
    constexpr       auto & data()       { return m_data; }

    // Bookkeeping; expose miscellaneous std::vector member functions
    constexpr void resize(size_t i) { m_data.resize(i);      }
    constexpr void erase(uint i)    { m_data.erase(i);       }
    constexpr void clear()          { m_data.clear();        }
    constexpr auto size()     const { return m_data.size();  }
    constexpr auto begin()    const { return m_data.begin(); }
    constexpr auto end()      const { return m_data.end();   }
    constexpr auto begin()          { return m_data.begin(); }
    constexpr auto end()            { return m_data.end();   }
  };
} // namespace met::detail